/*
 * apu_host — boot the game's own SPC700 driver from the ROM and render it.
 *
 * This is the reference implementation for route 1 ("the port plays the music
 * the way the console did"): the driver is uploaded from the ROM exactly as the
 * game uploads it, the CPU->APU command stream is replayed at its original
 * sample times, and the S-DSP produces the samples. No .spc snapshot, no PCM
 * asset, no sequencer rewrite.
 *
 * The upload format is the one the game uses (see the recomp's
 * RtlUploadSpcImageFromDpInternal): a stream of blocks
 *
 *     uint16 len, uint16 target, uint8 payload[len]   ... repeated
 *     uint16 0,   uint16 entry_pc                     (terminator)
 *
 * read from ROM at the pointer the game hands over (logged with
 * SNESRECOMP_SPC_UPLOAD_LOG=1). Addresses are LoROM (bank $80-$FF mirrors
 * $00-$7F): file offset = ((bank & 0x7F) << 15) | (addr & 0x7FFF).
 *
 * After the upload the APU is left exactly as the game's HLE leaves it: IPL
 * mapped out, ports cleared, A/X/Y = 0, SP = $EF if it was 0, PC = entry.
 *
 * Usage:
 *   apu_host rom.sfc --upload 84:8000 --seconds 60 --out driver.wav \
 *            [--ports cmd.txt] [--json]
 *
 * cmd.txt lines: "<sample_index> <port 0-3> <byte>" (0x-prefixed ok), which is
 * what tools/audio/dump_spc.py writes into ports.txt.
 */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apu.h"
#include "dsp.h"
#include "spc.h"
#include "apu_wav.h"

#define NATIVE_RATE 32040
#define MAX_BLOCKS 4096
#define MAX_PORTS 20000

typedef struct {
  uint16_t target;
  uint16_t len;
  uint32_t rom_off;
} UploadBlock;

static long lorom_offset(long bank, long addr) {
  return ((bank & 0x7F) << 15) | (addr & 0x7FFF);
}

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr,
            "usage: %s rom.sfc --upload BANK:ADDR --out out.wav [--seconds N]"
            " [--ports FILE] [--json] [--trace-every N]\n", argv[0]);
    return 2;
  }
  const char *rom_path = argv[1];
  const char *out_path = NULL;
  const char *ports_path = NULL;
  const char *aram_path = NULL;
  const char *io_spc_path = NULL;
  long upload_bank = -1, upload_addr = -1;
  long entry_override = -1;
  long port_offset = 0;
  int handshake = 0;
  double seconds = 60.0;
  bool json = false;
  int trace_every = 0;

  for (int i = 2; i < argc; i++) {
    if (!strcmp(argv[i], "--upload") && i + 1 < argc) {
      if (sscanf(argv[++i], "%li:%li", &upload_bank, &upload_addr) != 2) {
        fprintf(stderr, "apu_host: --upload wants BANK:ADDR (e.g. 84:8000)\n");
        return 2;
      }
    } else if (!strcmp(argv[i], "--aram") && i + 1 < argc) {
      aram_path = argv[++i];
    } else if (!strcmp(argv[i], "--handshake")) {
      handshake = 1;
    } else if (!strcmp(argv[i], "--echo-wait")) {
      handshake = 2;
    } else if (!strcmp(argv[i], "--port-offset") && i + 1 < argc) {
      port_offset = strtol(argv[++i], NULL, 0);
    } else if (!strcmp(argv[i], "--spc-io") && i + 1 < argc) {
      io_spc_path = argv[++i];
    } else if (!strcmp(argv[i], "--entry") && i + 1 < argc) {
      entry_override = strtol(argv[++i], NULL, 0);
    } else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
      out_path = argv[++i];
    } else if (!strcmp(argv[i], "--seconds") && i + 1 < argc) {
      seconds = atof(argv[++i]);
    } else if (!strcmp(argv[i], "--ports") && i + 1 < argc) {
      ports_path = argv[++i];
    } else if (!strcmp(argv[i], "--trace-every") && i + 1 < argc) {
      trace_every = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--json")) {
      json = true;
    } else {
      fprintf(stderr, "apu_host: unknown arg %s\n", argv[i]);
      return 2;
    }
  }
  if (!out_path || (upload_bank < 0 && !aram_path)) {
    fprintf(stderr, "apu_host: --upload BANK:ADDR or --aram FILE is required"
                    " (plus --out)\n");
    return 2;
  }
  if (aram_path && entry_override < 0) {
    fprintf(stderr, "apu_host: --aram needs --entry PC16\n");
    return 2;
  }

  FILE *rf = fopen(rom_path, "rb");
  if (!rf) {
    fprintf(stderr, "apu_host: cannot open %s\n", rom_path);
    return 1;
  }
  fseek(rf, 0, SEEK_END);
  long rom_size = ftell(rf);
  fseek(rf, 0, SEEK_SET);
  uint8_t *rom = malloc((size_t)rom_size);
  if (!rom || fread(rom, 1, (size_t)rom_size, rf) != (size_t)rom_size) {
    fprintf(stderr, "apu_host: cannot read %s\n", rom_path);
    return 1;
  }
  fclose(rf);

  /* ---- boot source: captured ARAM, or the ROM's upload stream ------ */
  if (aram_path) {
    Apu *apu = apu_init();
    if (!apu) {
      fprintf(stderr, "apu_host: apu_init failed\n");
      return 1;
    }
    FILE *af = fopen(aram_path, "rb");
    if (!af) {
      fprintf(stderr, "apu_host: cannot open %s\n", aram_path);
      return 1;
    }
    size_t got = fread(apu->ram, 1, 0x10000, af);
    fclose(af);
    if (got != 0x10000) {
      fprintf(stderr, "apu_host: %s is %zu bytes, expected 65536\n", aram_path,
              got);
      return 1;
    }
    /* A captured image is "post-boot": the driver owns the IO registers, so
     * the IPL is mapped out and the entry is the caller's choice. */
    memset(apu->inPorts, 0, 4);
    memset(apu->outPorts, 0, sizeof(apu->outPorts));
    apu->romReadable = false;
    apu->cpuCyclesLeft = 0;
    /* ARAM alone does not carry the timer configuration, and this driver's main
     * loop is `MOV A,$FD` (timer 0) + accumulate: without it the loop never
     * advances and the driver ignores every command. The .spc snapshot's
     * synthesized $F0-$FF region has exactly those values. */
    if (io_spc_path) {
      FILE *sf = fopen(io_spc_path, "rb");
      if (!sf) {
        fprintf(stderr, "apu_host: cannot open %s\n", io_spc_path);
        return 1;
      }
      static uint8_t spc_img[0x10200];
      size_t got_spc = fread(spc_img, 1, sizeof(spc_img), sf);
      fclose(sf);
      if (got_spc != sizeof(spc_img)) {
        fprintf(stderr, "apu_host: %s is %zu bytes, expected %zu\n",
                io_spc_path, got_spc, sizeof(spc_img));
        return 1;
      }
      const uint8_t *io = spc_img + 0x100 + 0xf0;
      uint8_t f1 = io[0x1];
      for (int i = 0; i < 3; i++) {
        apu->timer[i].enabled = (f1 >> i) & 1;
        apu->timer[i].target = io[0xa + i];
        apu->timer[i].counter = io[0xd + i] & 0x0f;
        apu->timer[i].divider = 0;
        apu->timer[i].cycles = 0;
      }
      apu->romReadable = (f1 & 0x80) != 0;
      apu->inPorts[0] = io[0x4];
      apu->inPorts[1] = io[0x5];
      apu->inPorts[2] = io[0x6];
      apu->inPorts[3] = io[0x7];
      apu->inPorts[4] = io[0x8];
      apu->inPorts[5] = io[0x9];
      if (getenv("APU_HOST_DEBUG"))
        fprintf(stderr, "io: f1=%02X timers=%d/%d/%d target=%d/%d/%d\n", f1,
                apu->timer[0].enabled, apu->timer[1].enabled,
                apu->timer[2].enabled, apu->timer[0].target,
                apu->timer[1].target, apu->timer[2].target);
    }
    apu->spc->a = 0;
    apu->spc->x = 0;
    apu->spc->y = 0;
    if (apu->spc->sp == 0)
      apu->spc->sp = 0xef;
    apu->spc->pc = (uint16_t)entry_override;

    int frames2 = (int)(seconds * NATIVE_RATE + 0.5);
    int16_t *pcm2 = calloc((size_t)frames2 * 2, sizeof(int16_t));
    if (!pcm2) {
      fprintf(stderr, "apu_host: out of memory\n");
      return 1;
    }
    int produced2 = 0, halted2 = -1, port_n2 = 0;
    int ack_ok = 0, ack_timeout = 0;
    long long ack_cycles = 0;
    int16_t chunk2[1024 * 2];
    static long ps[MAX_PORTS];
    static int pp[MAX_PORTS], pv[MAX_PORTS];
    int pcnt = 0;
    if (ports_path) {
      FILE *pf = fopen(ports_path, "r");
      if (pf) {
        char a[64], b[64], c[64];
        while (pcnt < MAX_PORTS && fscanf(pf, "%63s %63s %63s", a, b, c) == 3) {
          ps[pcnt] = strtol(a, NULL, 0);
          pp[pcnt] = (int)strtol(b, NULL, 0);
          pv[pcnt] = (int)strtol(c, NULL, 0);
          pcnt++;
        }
        fclose(pf);
      }
    }
    /* Drive the file from APU time, not from a fixed 32 cycles per sample: the
     * handshake burns cycles inside the write loop, and anything else would
     * compress the audio against the clock the driver actually sees. */
    while (produced2 < frames2) {
      int want = frames2 - produced2;
      if (want > 1024) want = 1024;
      for (int n = 0; n < want; n++) {
        while (port_n2 < pcnt && ps[port_n2] - port_offset <= produced2 + n) {
          uint8_t before[4];
          memcpy(before, apu->outPorts, sizeof(before));
          apu_writePortNow(apu, (uint8_t)pp[port_n2], (uint8_t)pv[port_n2]);
          port_n2++;
          /* The transfer is request/acknowledge: the CPU waits for the SPC to
           * answer on its output ports before it sends the next byte. Pacing by
           * the captured timestamps instead loses bytes and the driver never
           * starts; pacing by the acknowledge reproduces the console's side. */
          if (handshake) {
            /* Two pacing rules were tried: "any change on the output ports"
             * (handshake == 1) and "the SPC echoed the index we just wrote on
             * port 0" (handshake == 2). The IPL transfer is a request/echo
             * protocol, so the echo rule is the faithful one. */
            uint8_t want = (uint8_t)pv[port_n2 - 1];
            int port = pp[port_n2 - 1];
            /* Only the transfer's own writes are paced. The CPU also polls the
             * port while it waits (0xF9 written repeatedly); pacing those burns
             * the timeout and desyncs the transfer. A transfer byte announces
             * itself as the next index on port 0, and the block starts with $CC. */
            static int prev_port0 = -1;
            int is_index = (port == 0 && prev_port0 >= 0 &&
                            ((prev_port0 + 1) & 0xff) == want);
            if (port == 0) prev_port0 = want;
            /* $CC on port 0 starts an IPL transfer: the driver answers with the
             * ready pair $AA/$BB on its own output ports (the oracle's
             * apu_waitForTransferReady uses exactly this test). */
            /* $CC opens a block, but this driver does not answer with the IPL
             * ROM's $AA/$BB ready pair (post-boot it uses its own routine), so
             * it is written without waiting. */
            int is_start = 0;
            int w = 0;
            for (; w < 400000; w++) {
              apu_cycle(apu);
              if (is_start) {
                if (apu->outPorts[0] == 0xaa && apu->outPorts[1] == 0xbb) break;
              } else if (handshake == 1) {
                if (memcmp(before, apu->outPorts, sizeof(before))) break;
              } else if (is_index) {
                if (apu->outPorts[0] == want) break;
              } else {
                break;
              }
            }
            ack_cycles += w;
            if (w >= 400000) ack_timeout++; else ack_ok++;
          }
        }
        for (int k = 0; k < 32; k++) apu_cycle(apu);
        if (apu->spc->stopped && halted2 < 0) halted2 = produced2;
        /* Emit every sample the DSP produced, right away: the handshake can
         * burn arbitrary cycles, and batching later would stretch the file
         * against the clock the driver and the DSP actually share. */
        while (produced2 < frames2 && dsp_available(apu->dsp) > 0) {
          dsp_getSamples(apu->dsp, &pcm2[(size_t)produced2 * 2], 1);
          produced2++;
        }
      }
      if (produced2 >= frames2) break;
    }
    (void)chunk2;
    double sL = 0, sR = 0;
    int pL = 0, pR = 0;
    for (int i = 0; i < produced2; i++) {
      sL += (double)pcm2[i * 2] * pcm2[i * 2];
      sR += (double)pcm2[i * 2 + 1] * pcm2[i * 2 + 1];
      if (abs(pcm2[i * 2]) > pL) pL = abs(pcm2[i * 2]);
      if (abs(pcm2[i * 2 + 1]) > pR) pR = abs(pcm2[i * 2 + 1]);
    }
    if (apu_write_wav(out_path, pcm2, produced2, NATIVE_RATE) != 0) {
      fprintf(stderr, "apu_host: cannot write %s\n", out_path);
      return 1;
    }
    if (json)
      printf("{\"ok\":true,\"aram\":\"%s\",\"entry_pc\":%ld,"
             "\"frames\":%d,\"seconds\":%.3f,\"halted_at\":%d,"
             "\"peak_l\":%d,\"peak_r\":%d,\"rms_l\":%.1f,\"rms_r\":%.1f,"
             "\"port_writes\":%d,\"ack_ok\":%d,\"ack_timeout\":%d,"
             "\"ack_cycles\":%lld,\"wav\":\"%s\"}\n",
             aram_path, entry_override, produced2,
             (double)produced2 / NATIVE_RATE, halted2, pL, pR,
             produced2 ? sqrt(sL / produced2) : 0,
             produced2 ? sqrt(sR / produced2) : 0, port_n2, ack_ok, ack_timeout,
             ack_cycles, out_path);
    else
      printf("aram %s entry 0x%04lX: rendered %.2f s, peak %d/%d rms %.1f/%.1f,"
             " halted_at=%d\n", aram_path, entry_override,
             (double)produced2 / NATIVE_RATE, pL, pR,
             produced2 ? sqrt(sL / produced2) : 0,
             produced2 ? sqrt(sR / produced2) : 0, halted2);
    free(pcm2);
    apu_free(apu);
    free(rom);
    return halted2 >= 0 ? 3 : 0;
  }

  /* ---- parse the upload stream ------------------------------------- */
  long pos = lorom_offset(upload_bank, upload_addr);
  if (pos < 0 || pos + 4 > rom_size) {
    fprintf(stderr, "apu_host: upload pointer outside the ROM\n");
    return 1;
  }
  static UploadBlock blocks[MAX_BLOCKS];
  int nblocks = 0;
  uint16_t entry = 0;
  long stream_start = pos;
  for (;;) {
    if (pos + 4 > rom_size) {
      fprintf(stderr, "apu_host: upload stream runs off the ROM\n");
      return 1;
    }
    uint16_t len = (uint16_t)(rom[pos] | (rom[pos + 1] << 8));
    uint16_t target = (uint16_t)(rom[pos + 2] | (rom[pos + 3] << 8));
    pos += 4;
    if (len == 0) {
      entry = target;
      break;
    }
    if (pos + len > rom_size || nblocks >= MAX_BLOCKS) {
      fprintf(stderr, "apu_host: malformed upload stream\n");
      return 1;
    }
    blocks[nblocks].target = target;
    blocks[nblocks].len = len;
    blocks[nblocks].rom_off = (uint32_t)pos;
    nblocks++;
    pos += len;
  }
  long stream_bytes = pos - stream_start;

  /* ---- boot the APU with it ---------------------------------------- */
  Apu *apu = apu_init();
  if (!apu) {
    fprintf(stderr, "apu_host: apu_init failed\n");
    return 1;
  }
  long written = 0;
  for (int i = 0; i < nblocks; i++) {
    memcpy(&apu->ram[blocks[i].target], &rom[blocks[i].rom_off], blocks[i].len);
    written += blocks[i].len;
  }
  /* The game's HLE leaves the APU in this state after the boot upload. */
  memset(apu->inPorts, 0, 4);
  memset(apu->outPorts, 0, sizeof(apu->outPorts));
  apu->romReadable = false;
  apu->cpuCyclesLeft = 0;
  apu->spc->a = 0;
  apu->spc->x = 0;
  apu->spc->y = 0;
  if (apu->spc->sp == 0)
    apu->spc->sp = 0xef;
  apu->spc->pc = entry;

  /* ---- optional CPU->APU command replay ---------------------------- */
  static long port_s[MAX_PORTS];
  static int port_p[MAX_PORTS], port_v[MAX_PORTS];
  int port_count = 0;
  if (ports_path) {
    FILE *pf = fopen(ports_path, "r");
    if (!pf) {
      fprintf(stderr, "apu_host: cannot open %s\n", ports_path);
      return 1;
    }
    char a[64], b[64], c[64];
    while (port_count < MAX_PORTS &&
           fscanf(pf, "%63s %63s %63s", a, b, c) == 3) {
      port_s[port_count] = strtol(a, NULL, 0);
      port_p[port_count] = (int)strtol(b, NULL, 0);
      port_v[port_count] = (int)strtol(c, NULL, 0);
      port_count++;
    }
    fclose(pf);
  }

  int frames = (int)(seconds * NATIVE_RATE + 0.5);
  int16_t *pcm = calloc((size_t)frames * 2, sizeof(int16_t));
  if (!pcm) {
    fprintf(stderr, "apu_host: out of memory\n");
    return 1;
  }
  int produced = 0, port_n = 0, halted_at = -1;
  int16_t chunk[1024 * 2];
  while (produced < frames) {
    int want = frames - produced;
    if (want > 1024) want = 1024;
    for (int n = 0; n < want; n++) {
      while (port_n < port_count && port_s[port_n] <= produced + n) {
        apu_writePortNow(apu, (uint8_t)port_p[port_n], (uint8_t)port_v[port_n]);
        port_n++;
      }
      for (int k = 0; k < 32; k++) apu_cycle(apu);
      if (apu->spc->stopped && halted_at < 0) halted_at = produced + n;
    }
    uint32_t got = dsp_getSamples(apu->dsp, chunk, want);
    if (got == 0) break;
    memcpy(&pcm[(size_t)produced * 2], chunk,
           (size_t)got * 2 * sizeof(int16_t));
    produced += (int)got;
    if (trace_every > 0 && produced / trace_every !=
                               (produced + (int)got) / trace_every)
      fprintf(stderr, "[apu t=%.2fs] pc=%04X stopped=%d a=%02X x=%02X y=%02X\n",
              (double)(produced + (int)got) / NATIVE_RATE, apu->spc->pc,
              (int)apu->spc->stopped, apu->spc->a, apu->spc->x, apu->spc->y);
  }

  double sumL = 0, sumR = 0;
  int peakL = 0, peakR = 0;
  for (int i = 0; i < produced; i++) {
    sumL += (double)pcm[i * 2] * pcm[i * 2];
    sumR += (double)pcm[i * 2 + 1] * pcm[i * 2 + 1];
    if (abs(pcm[i * 2]) > peakL) peakL = abs(pcm[i * 2]);
    if (abs(pcm[i * 2 + 1]) > peakR) peakR = abs(pcm[i * 2 + 1]);
  }
  if (apu_write_wav(out_path, pcm, produced, NATIVE_RATE) != 0) {
    fprintf(stderr, "apu_host: cannot write %s\n", out_path);
    return 1;
  }

  if (json) {
    printf("{\"ok\":true,\"rom\":\"%s\",\"upload\":\"%li:%li\","
           "\"stream_bytes\":%ld,\"blocks\":%d,\"aram_bytes\":%ld,"
           "\"entry_pc\":%u,\"frames\":%d,\"seconds\":%.3f,"
           "\"halted_at\":%d,\"peak_l\":%d,\"peak_r\":%d,"
           "\"rms_l\":%.1f,\"rms_r\":%.1f,\"port_writes\":%d,\"wav\":\"%s\"}\n",
           rom_path, upload_bank, upload_addr, stream_bytes, nblocks, written,
           (unsigned)entry, produced, (double)produced / NATIVE_RATE, halted_at,
           peakL, peakR, produced ? sqrt(sumL / produced) : 0,
           produced ? sqrt(sumR / produced) : 0, port_n, out_path);
  } else {
    printf("upload %li:%li: %d blocks, %ld bytes, entry 0x%04X\n",
           upload_bank, upload_addr, nblocks, written, (unsigned)entry);
    printf("rendered %.2f s, peak %d/%d rms %.1f/%.1f, halted_at=%d\n",
           (double)produced / NATIVE_RATE, peakL, peakR,
           produced ? sqrt(sumL / produced) : 0,
           produced ? sqrt(sumR / produced) : 0, halted_at);
  }

  free(pcm);
  apu_free(apu);
  free(rom);
  return halted_at >= 0 ? 3 : 0;
}
