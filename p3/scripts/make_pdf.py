#!/usr/bin/env python3
"""Convierte p3/memoria/memoria.md a HTML y a PDF (via chromium headless)."""
import os, subprocess, shutil, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MEM = os.path.join(ROOT, "memoria")
MD = os.path.join(MEM, "memoria.md")
HTML = os.path.join(MEM, "memoria.html")
PDF = os.path.join(MEM, "memoria.pdf")

import markdown

with open(MD, encoding="utf-8") as f:
    text = f.read()

body = markdown.markdown(text, extensions=["tables", "fenced_code", "toc", "sane_lists"])

css = """
<meta charset="utf-8">
<style>
  body { font-family: 'DejaVu Sans', Arial, sans-serif; font-size: 11px; line-height: 1.45;
         margin: 28px 34px; color: #1a202c; }
  h1 { font-size: 22px; border-bottom: 2px solid #2b6cb0; padding-bottom: 4px; }
  h2 { font-size: 17px; margin-top: 22px; border-bottom: 1px solid #cbd5e0; padding-bottom: 3px; color: #2b6cb0; }
  h3 { font-size: 14px; margin-top: 16px; color: #2c5282; }
  table { border-collapse: collapse; margin: 8px 0; width: 100%; }
  th, td { border: 1px solid #a0aec0; padding: 3px 6px; text-align: left; }
  th { background: #e2e8f0; }
  code, pre { font-family: 'DejaVu Sans Mono', monospace; background: #f7fafc; }
  pre { border: 1px solid #e2e8f0; padding: 8px; overflow-x: auto; }
  blockquote { border-left: 3px solid #f6ad55; background: #fffaf0; margin: 8px 0; padding: 4px 12px; }
  img { max-width: 100%; }
  h1, h2, h3 { page-break-after: avoid; }
  table, pre, img { page-break-inside: avoid; }
</style>
"""
html = f"<!doctype html><html><head>{css}</head><body>{body}</body></html>"
with open(HTML, "w", encoding="utf-8") as f:
    f.write(html)

browser = shutil.which("chromium") or shutil.which("chromium-browser") or shutil.which("google-chrome")
if not browser:
    print("No se encontro chromium; se ha generado solo el HTML:", HTML)
    sys.exit(0)

cmd = [browser, "--headless", "--disable-gpu", "--no-sandbox",
       f"--print-to-pdf={PDF}", "--no-pdf-header-footer", f"file://{HTML}"]
subprocess.run(cmd, check=True)
print("PDF ->", PDF if os.path.exists(PDF) else "(no generado)")
