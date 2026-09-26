# Design references

Mockups generated with Higgsfield (GPT Image 2.5), each using a render of the built panel as its layout reference. They are direction references only. The text inside their displays is the image model's guess and is wrong in places. The real interface is drawn in code in `plugin/src/gui/`.

| File | Direction | Outcome |
|---|---|---|
| `mockup-01-hardware.png` | 70s studio hardware: black anodised metal, knurled knobs, walnut cheeks | Rejected: too vintage |
| `mockup-02a-digital.png` | Flat digital: graphite, monospace, orange accent, LED-ring encoders | Not used: it scrambled the selector rows |
| `mockup-02b-digital.png` | Same direction, correct control structure | **Built**, pushed further toward black and orange |
| `mockup-03a-presets.png`, `mockup-03b-presets.png` | The 1100 x 800 panel with the preset strip, A/B, and the modern row, from a render of the built panel | Generated; not yet compared with the build. Download pending (see below) |

The 03 mockups are Higgsfield jobs `4f8296e7-4741-4fa0-8f60-4a65a702010b` and `4e2e358c-7a62-4f86-b9f3-9d5756d2e43b`. The cloud session that made them couldn't fetch Higgsfield's image host, so they aren't in this folder yet. On the Mac:

```bash
curl -o docs/design/mockup-03a-presets.png https://d8j0ntlcm91z4.cloudfront.net/user_3IyLlQd2qvmni91SeCQnBnmEegj/hf_20260926_021228_4f8296e7-4741-4fa0-8f60-4a65a702010b.png
curl -o docs/design/mockup-03b-presets.png https://d8j0ntlcm91z4.cloudfront.net/user_3IyLlQd2qvmni91SeCQnBnmEegj/hf_20260926_021229_4e2e358c-7a62-4f86-b9f3-9d5756d2e43b.png
```

To check the real interface against these, render it:

```bash
build/plugin/the89th-snapshot out.png 1100 pitch     # or delay, freeze, quasi, modern
```
