# Tooling

## Extracting frames from a video

`tools/video_frames.py` pulls frames out of any mp4/mkv using ffmpeg (installed as
a Python wheel, no system install needed):

```
python tools/video_frames.py "C:\path\take.mp4" --every 20 --sheet
```

- `--every N` keeps one frame in N (default 10), `--fps F` asks for F per second;
- `--start/--end SEC` limit the range, `--scale W` downscales;
- `--sheet` writes `contact_sheet.png` with frame numbers, so a whole take can be
  skimmed at once.

Frames land in `<video-name>/frame_00001.png`. Requires
`pip install imageio-ffmpeg` (the tool says so if it is missing).
