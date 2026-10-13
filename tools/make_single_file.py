"""make_single_file.py - build helper (not part of the program logic).
Puts the Emscripten single-file JavaScript (which already contains the
WebAssembly as base64) inside index.html, giving ONE offline HTML file."""
import sys

html_path, js_path, out_path = sys.argv[1:4]
html = open(html_path, encoding="utf-8").read()
js = open(js_path, encoding="utf-8").read().replace("</script", "<\\/script")
tag = '<script src="ortho.js"></script>'
assert tag in html, "script tag not found in index.html"
html = html.replace(tag, "<script>\n" + js + "\n</script>")
open(out_path, "w", encoding="utf-8").write(html)
print("wrote", out_path, len(html) // 1024, "KB")
