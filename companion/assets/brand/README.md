# Branding

Original SVGs by **@kapeywu on Discord**, who also made the Cavalry splash animation.

Regenerate the web and native assets from the repository root:

```sh
uv run tools/companion/generate_brand_assets.py
```

Android launcher padding lives in `mipmap-anydpi-v26/ic_launcher*.xml`, not in
the foreground PNGs. The generator uses the same inset for legacy icons.
