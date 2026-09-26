# Design references

Mockups generated with Higgsfield (GPT Image 2.5), each using a render of the built panel as its layout reference. They are direction references only. The text inside their displays is the image model's guess and is wrong in places. The real interface is drawn in code in `plugin/src/gui/`.

| File | Direction | Outcome |
|---|---|---|
| `mockup-01-hardware.png` | 70s studio hardware: black anodised metal, knurled knobs, walnut cheeks | Rejected: too vintage |
| `mockup-02a-digital.png` | Flat digital: graphite, monospace, orange accent, LED-ring encoders | Not used: it scrambled the selector rows |
| `mockup-02b-digital.png` | Same direction, correct control structure | **Built**, pushed further toward black and orange |

To check the real interface against these, render it:

```bash
build/plugin/the89th-snapshot out.png 1100 pitch
```
