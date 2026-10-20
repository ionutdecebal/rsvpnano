# Classic update selector candidate

This branch retains the Classic interface. The Rev1 update screen checks the
current Classic channel, or explicitly checks New UI before a second confirmation.
Only LCD 3.49 Rev1 is built and packaged for this local acceptance pass.
Other boards retain their existing release-based update route and are not offered New UI.

The updater streams certificate-verified HTTPS to the inactive app slot, checks
length and SHA-256, validates the image and saves switching settings before boot
selection. A custom source is retained for normal updates; explicit interface
switching backs it up in NVS and follows official updates thereafter.
Saved New UI `chunks` mode is accepted as single-word RSVP. Shared speed and
Wi-Fi survive native codec tests. Keep an SD backup: Classic can discard settings
that only the New UI understands. Books and recordings are not removed.

The public catalog is not yet deployed. Do not publish this candidate as the
stable maintenance release until physical update/return tests pass. Publish the
accepted revision with a unique stable Classic tag and an `update-channel.json`
descriptor; the Pages staging script uses only descriptor-equipped Classic as a
return target. The New UI public version remains v0.2.0 and a separate prerelease.

Use `tools/make_update_descriptor.py --image <ota.bin> --channel classic
--tag <immutable-tag> --commit <full-sha> --output update-channel.json` after the
clean tagged build. Keep merged and application-only binaries separate. The
Classic workflow now refuses release replacement and tag moves; its automatic
publication is restricted to v0.0.x/v0.1.x. Do not push a stable release tag during
candidate testing.
