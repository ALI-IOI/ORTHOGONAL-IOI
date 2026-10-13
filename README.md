# Orthogonal - IOI

**From Curves to Solutions: Exploring Differential Equations**
MATH 205 · Differential Equations, Laplace Transformation and Fourier Transformations
BAUST Khulna · CSE–3A · Group 7 · [Group Name]
Instructor: Pulak Kundu, Lecturer, Department of Mathematics

You type a one-parameter family of curves, such as `y = c x^2`, and the tool:

1. checks that the input is a real family of curves (or shows *"Invalid information: …"*),
2. differentiates the family, eliminates `c` and gets the family's differential equation,
3. replaces dy/dx by −1/(dy/dx) to get the orthogonal differential equation,
4. solves it in closed form when the equation is separable (and checks the answer), or numerically with RK4,
5. draws both families on a Desmos-style graph that you can zoom and pan, with an animation,
6. measures the angle at every crossing to confirm the curves are perpendicular.

**All the mathematics is C++** (folder `src/`), compiled to WebAssembly. JavaScript in
`web/page.html` (built into `index.html`) only reads the input boxes, calls the C++ function `simulate()` and draws
the returned points on a canvas. A browser cannot draw on the screen or read input boxes from
WebAssembly alone, so this small amount of JavaScript is the minimum needed.

---

**Live site:** https://ali-ioi.github.io/ORTHOGONAL-IOI/
**New here? Read [INSTRUCTIONS.md](INSTRUCTIONS.md).**

## Folder structure

```
index.html          the whole website in ONE file (page + JavaScript glue + WebAssembly).
                    GitHub Pages serves it; it also works offline by double-click.
src/                C++ source (all the logic)
  Tokenizer.*         scanner (a small DFA): text -> tokens
  Parser.*            shunting-yard algorithm with two stacks: tokens -> expression tree
  Expr.*              expression tree: Node base class + Num, Var, Neg, Add, Sub, Mul, Div, Pow, Func
  Simplifier.*        tidies expressions (sum of terms, merges powers) so results read like a textbook
  Differentiator.h    partial derivatives (rules live in each node's derive())
  FamilyAnalyzer.*    F(x,y,c), eliminate c, family DE, orthogonal DE
  Solver.*            ClosedFormSolver (separable equations) + RK4Tracer
  CurveGenerator.*    family curves (sampling / marching squares), trajectories, perpendicularity check
  Validator.*         checks every input box
  Api.*               simulate(): runs everything and returns JSON to the page
  Types.h             small shared types (Box, Polyline, Step)
web/page.html       page layout + minimal JavaScript (the source of index.html)
tests/test_main.cpp native test program (examples + invalid inputs); tests/test_output.txt = last run
docs/Code-Walkthrough.docx   plain-English explanation of every file for the viva
build.sh            builds index.html with Emscripten
build_native_tests.sh  builds and runs the tests with g++
tools/make_single_file.py  build helper that puts the JavaScript and WebAssembly inside the HTML
```

## Use it

- **Online:** https://ali-ioi.github.io/ORTHOGONAL-IOI/
- **Offline:** **Code → Download ZIP**, unzip, double-click `index.html`. No internet needed
  (only the web fonts fall back to the system fonts).

Works on Windows, macOS, Linux, Android, iPhone/iPad and ChromeOS in a current browser:
Chrome/Edge 85+, Firefox 79+, Safari 15+, Samsung Internet. Mouse wheel / pinch to zoom, drag to move.

## Build

1. Install Emscripten (once):
   ```bash
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ./emsdk install latest
   ./emsdk activate latest
   source ./emsdk_env.sh        # on Windows: emsdk_env.bat
   ```
2. Build: `./build.sh` → writes `index.html`.
3. Test (needs only g++): `./build_native_tests.sh`.
   On Windows, run the scripts in Git Bash or WSL.

## Publish on GitHub Pages

**Settings → Pages → Deploy from a branch → `main` / `/ (root)` → Save.** After about a minute the
site is live at `https://<username>.github.io/<repository>/`.

## Input format

| Box | Example | Notes |
|---|---|---|
| Type | Explicit / Implicit | explicit: `y = f(x, c)`; implicit: `F(x, y, c) = G(x, y, c)` |
| Equation | `y = c x^2`, `x^2 + y^2 = c`, `xy = c` | `^` power, `2x` = 2·x, functions `sin cos tan exp ln sqrt abs asin acos atan sinh cosh tanh`, constants `pi`, `e`. `log` means natural log. |
| Parameter | `c` | one letter, not x, y or e |
| Values | `-3 to 3` or `1, 2, 4` | a range uses "Number of curves" |
| View | x from −5 to 5, y from −4 to 4 | scales are kept equal; the shorter side is widened |
| Orthogonal curves | 12 | 1–60 |

## Test results (from `tests/test_output.txt`)

| Family | Closed form found by the program | Crossings checked | Largest abs(cos θ) |
|---|---|---|---|
| y = c x² | x² + 2y² = k | 56 | 0.025 |
| x² + y² = c | ln‖y‖ − ln‖x‖ = K ⇒ y = kx | 119 | ≈ 0 (6e-15) |
| xy = c | x² − y² = k | 73 | 0.0063 |
| y = c eˣ | 2x + y² = k (same as y² = k − 2x) | 71 | 0.016 |
| y = cx | x² + y² = k | 43 | 0.016 |
| y² = 4cx | 2x² + y² = k | 48 | 0.022 |
| (x − c)² + y² = 1 (c is non-linear) | none, RK4 only | 24 | 0.0068 |

abs(cos θ) is computed at each crossing between the RK4 trajectory (tangent measured from its
points) and the exact tangent of the family curve. 0 would mean exactly 90°; 0.025 means
88.6°. The small error comes from the step size of the drawing, not from the formulas. The
check skips crossings at singular points. These are points where every curve of the family
meets, such as the origin for y = c x², or points where the slope is undefined.

All 15 invalid-input tests show a clear message, for example:
`Invalid information: Unbalanced brackets: '(' at position 6 is never closed.`

## Limits (stated honestly)

- Closed forms are found only for separable equations dy/dx = P(x)·Q(y), using a small table of
  integrals (powers, 1/x, eˣ, sin, cos, linear arguments). Every closed form is checked by
  differentiating it again at 80 points. Other equations are solved and drawn with RK4 only.
- When c enters non-linearly and cannot be isolated, c is found at each point with Newton's method.
- Polar families (r, θ) are not supported. Type them in x and y instead.
