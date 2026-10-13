# How to use Orthogonal - IOI

A step-by-step guide. No coding needed for steps 1–4.

**Live site:** https://ali-ioi.github.io/ORTHOGONAL-IOI/

---

## 1. Open it

- **On any device with internet:** open the live site link above. Bookmark it on your laptop and phone.
- **Without internet (for the classroom):** on GitHub click **Code → Download ZIP**, unzip it and
  double-click `index.html`. Everything works offline, because the C++ engine is inside that one file.

## 2. Enter a family of curves

The form is on the right (on phones and tablets it is below the graph). Fill it from top to bottom:

| Box | What to type | Example |
|---|---|---|
| **1 Type of equation** | *Explicit* if the question is `y = …`; *Implicit* for anything else | `x^2 + y^2 = c` → Implicit |
| **2 Equation of the family** | The family with its parameter. `^` is a power, `2x` means 2·x | `y = c x^2` |
| **3 Name of the parameter** | One letter, usually `c` or `k` | `c` |
| **4 Values of the parameter** | A range `a to b`, or a list `1, 2, 4` | `-3 to 3` |
| **5 Number of curves** | Only for a range: how many curves to draw | `7` |
| **6 View range** | The part of the plane to show | x from −5 to 5, y from −4 to 4 |
| **7 Number of orthogonal curves** | How many trajectories to draw (optional) | `12` |
| **8 Colours** | Optional | — |

Allowed in an equation: `x`, `y`, the parameter, numbers, `+ - * / ^ ( )`, `pi`, `e`, and
`sin cos tan exp ln sqrt abs asin acos atan sinh cosh tanh` (`log` means natural log).
Symbols copied from Word or a PDF (−, ×, ², π) are understood too.

Not sure? Click one of the **Examples** to fill the form.

## 3. Press Simulate

- If something is wrong, a box says **Invalid information: …** and tells you exactly what to fix
  (an empty box, a missing parameter, unbalanced brackets, an equation that is one curve and not a family, …).
- If everything is right, a green line says **The information is valid**, and the curves are drawn as an animation:
  the family first (green), then the orthogonal trajectories (orange).

## 4. Read and explore the result

- **Graph:** drag to move, mouse wheel or two-finger pinch to zoom, **+ / −** buttons, **▶** to replay the
  animation, **Reset** to go back.
- **Steps derived by the program** (under the graph): the family's differential equation, the orthogonal
  equation (dy/dx replaced by −1/(dy/dx)), and the solution: a closed form when the equation is separable
  (checked by the program), otherwise "solved with RK4".
- **Perpendicularity check:** the program finds the crossings of the two families and measures the angle.
  A largest |cos θ| near 0 means the tangents meet at 90°.

## 5. Understand the code

- `docs/Code-Walkthrough.docx` explains every C++ file and important function in plain English.
- Run the tests yourself: `./build_native_tests.sh` (needs g++). The last output is in `tests/test_output.txt`.

## 6. Change the code and rebuild (light coding)

1. Edit the C++ in `src/` (or the page in `web/page.html`).
2. Run `./build_native_tests.sh` to check the maths.
3. Run `./build.sh` (needs Emscripten, see README) to make a new `index.html`.
4. Commit and push; GitHub Pages updates in about a minute.

## Troubleshooting

| Problem | Fix |
|---|---|
| The site shows a 404 | Pages takes a minute after a push. Check **Settings → Pages** says "Your site is live". |
| "Loading the C++ engine…" never ends | Update the browser (Chrome/Edge 85+, Firefox 79+, Safari 15+). |
| "No curve of the family is visible" | Change the values of c (for example `x^2 + y^2 = c` needs c > 0) or widen the view. |
| The trajectories stop at a point | That is a singular point (for example the origin for y = c x², where every curve meets). The direction is undefined there. |
| No closed form is shown | The orthogonal equation is not separable or needs an integral outside the program's table; the curves are still drawn correctly with RK4. |
