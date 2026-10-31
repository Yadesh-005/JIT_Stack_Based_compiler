"""Helpers that reproduce the Review 2 report formatting on top of the
Review 2 .docx used as the template (styles, header, footer, margins)."""
import copy
import re

from docx import Document
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_BREAK
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor

DARK = "1F4E78"
MID = "2E74B5"


class Report:
    def __init__(self, template, header_text):
        self.doc = Document(template)
        body = self.doc.element.body
        for child in list(body):
            if child.tag != qn("w:sectPr"):
                body.remove(child)
        hdr = self.doc.sections[0].header
        for p in hdr.paragraphs:
            for r in p.runs:
                if "Review 2" in r.text:
                    r.text = r.text.replace("Review 2", "Review 3")
        self.headings = []  # (level, text, bookmark)
        self._bm = 100
        self.fig = 0
        self.tab = 0
        self.toc_pages = {}

    # ------------------------------------------------------------- basics
    def _runs(self, p, text, size=None, color=None, bold=False, italic=False, mono_size=10):
        """Inline markup: **bold**, `code`, *italic*."""
        parts = re.split(r"(\*\*[^*]+\*\*|`[^`]+`|\*[^*]+\*)", text)
        for part in parts:
            if not part:
                continue
            b, it, mono = bold, italic, False
            if part.startswith("**") and part.endswith("**"):
                part, b = part[2:-2], True
            elif part.startswith("`") and part.endswith("`"):
                part, mono = part[1:-1], True
            elif part.startswith("*") and part.endswith("*") and len(part) > 2:
                part, it = part[1:-1], True
            r = p.add_run(part)
            r.bold = b
            r.italic = it
            if mono:
                r.font.name = "Consolas"
                r._element.rPr.rFonts.set(qn("w:eastAsia"), "Consolas")
                r.font.size = Pt(mono_size)
            elif size:
                r.font.size = Pt(size)
            if color:
                r.font.color.rgb = RGBColor.from_string(color)
        return p

    def para(self, text, align="both", after=160, before=0, size=None, color=None, bold=False, italic=False,
             keep_next=False):
        p = self.doc.add_paragraph()
        pf = p.paragraph_format
        pf.space_after = Pt(after / 20)
        pf.space_before = Pt(before / 20)
        pf.line_spacing = 1.15
        pf.keep_with_next = keep_next
        p.alignment = {"both": WD_ALIGN_PARAGRAPH.JUSTIFY, "center": WD_ALIGN_PARAGRAPH.CENTER,
                       "left": WD_ALIGN_PARAGRAPH.LEFT, "right": WD_ALIGN_PARAGRAPH.RIGHT}[align]
        self._runs(p, text, size=size, color=color, bold=bold, italic=italic)
        return p

    def _bookmark(self, p):
        self._bm += 1
        name = f"_r3h{self._bm}"
        start = OxmlElement("w:bookmarkStart")
        start.set(qn("w:id"), str(self._bm))
        start.set(qn("w:name"), name)
        end = OxmlElement("w:bookmarkEnd")
        end.set(qn("w:id"), str(self._bm))
        p._p.insert(1 if p._p.pPr is not None else 0, start)
        p._p.append(end)
        return name

    def heading(self, text, level=1, page_break=False, toc=True):
        if page_break:
            self.page_break()
        p = self.doc.add_paragraph(style=f"Heading {level}")
        p.add_run(text)
        p.paragraph_format.keep_with_next = True
        bm = self._bookmark(p)
        if toc:
            self.headings.append((level, text, bm))
        return p

    def h1(self, text, page_break=False):
        return self.heading(text, 1, page_break)

    def h2(self, text):
        return self.heading(text, 2)

    def h3(self, text):
        return self.heading(text, 3, toc=False)

    def bullet(self, text, level=0):
        p = self.doc.add_paragraph()
        pPr = p._p.get_or_add_pPr()
        numPr = OxmlElement("w:numPr")
        il = OxmlElement("w:ilvl")
        il.set(qn("w:val"), str(level))
        nid = OxmlElement("w:numId")
        nid.set(qn("w:val"), "1")
        numPr.append(il)
        numPr.append(nid)
        pPr.append(numPr)
        p.paragraph_format.space_after = Pt(3)
        p.paragraph_format.line_spacing = 1.15
        p.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
        self._runs(p, text)
        return p

    def numbered(self, items):
        for i, t in enumerate(items, 1):
            p = self.para(f"**{i}.** " + t, after=60)
            p.paragraph_format.left_indent = Inches(0.3)
            p.paragraph_format.first_line_indent = Inches(-0.22)

    def page_break(self):
        p = self.doc.add_paragraph()
        p.add_run().add_break(WD_BREAK.PAGE)

    def code(self, text, size=8.5):
        lines = text.rstrip("\n").split("\n")
        for i, line in enumerate(lines):
            p = self.doc.add_paragraph()
            pf = p.paragraph_format
            pf.space_after = Pt(0)
            pf.space_before = Pt(0)
            pf.line_spacing = 1.0
            pf.keep_together = True
            if i < len(lines) - 1:
                pf.keep_with_next = len(lines) <= 40
            pPr = p._p.get_or_add_pPr()
            bdr = OxmlElement("w:pBdr")
            sides = ["left", "right"] + (["top"] if i == 0 else []) + (["bottom"] if i == len(lines) - 1 else [])
            for side in ["top", "left", "bottom", "right"]:
                if side in sides:
                    e = OxmlElement(f"w:{side}")
                    e.set(qn("w:val"), "single")
                    e.set(qn("w:sz"), "4")
                    e.set(qn("w:space"), "4")
                    e.set(qn("w:color"), "CCCCCC")
                    bdr.append(e)
            pPr.append(bdr)
            shd = OxmlElement("w:shd")
            shd.set(qn("w:val"), "clear")
            shd.set(qn("w:color"), "auto")
            shd.set(qn("w:fill"), "F2F2F2")
            pPr.append(shd)
            r = p.add_run(line if line else " ")
            r.font.name = "Consolas"
            r._element.rPr.rFonts.set(qn("w:eastAsia"), "Consolas")
            r._element.rPr.rFonts.set(qn("w:hAnsi"), "Consolas")
            r.font.size = Pt(size)
        sp = self.doc.add_paragraph()
        sp.paragraph_format.space_after = Pt(4)
        sp.paragraph_format.line_spacing = 0.6

    def caption(self, text):
        p = self.para(text, align="center", after=200, size=9.5, italic=True, color="404040")
        return p

    def figure(self, path, width, caption):
        p = self.doc.add_paragraph()
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        p.paragraph_format.keep_with_next = True
        p.paragraph_format.space_after = Pt(4)
        p.add_run().add_picture(path, width=Inches(width))
        self.fig += 1
        self.caption(f"Figure {self.fig}: {caption}")

    # ------------------------------------------------------------- tables
    @staticmethod
    def _cell_shade(cell, fill):
        tcPr = cell._tc.get_or_add_tcPr()
        shd = OxmlElement("w:shd")
        shd.set(qn("w:val"), "clear")
        shd.set(qn("w:color"), "auto")
        shd.set(qn("w:fill"), fill)
        tcPr.append(shd)

    @staticmethod
    def _cell_margins(table):
        tblPr = table._tbl.tblPr
        mar = OxmlElement("w:tblCellMar")
        for side, w in (("top", 50), ("left", 100), ("bottom", 50), ("right", 100)):
            e = OxmlElement(f"w:{side}")
            e.set(qn("w:w"), str(w))
            e.set(qn("w:type"), "dxa")
            mar.append(e)
        tblPr.append(mar)

    @staticmethod
    def _borders(table):
        tblPr = table._tbl.tblPr
        b = OxmlElement("w:tblBorders")
        for side in ("top", "left", "bottom", "right", "insideH", "insideV"):
            e = OxmlElement(f"w:{side}")
            e.set(qn("w:val"), "single")
            e.set(qn("w:sz"), "6")
            e.set(qn("w:space"), "0")
            e.set(qn("w:color"), "000000")
            b.append(e)
        tblPr.append(b)

    def table(self, headers, rows, widths, size=9.5, caption=None, header_fill=DARK, bold_first_col=False,
              center_cols=()):
        if caption:
            self.tab += 1
            cp = self.para(f"Table {self.tab}: {caption}", align="left", after=60, size=9.5, italic=True,
                           color="404040", keep_next=True)
        t = self.doc.add_table(rows=1 + len(rows), cols=len(headers))
        t.alignment = WD_TABLE_ALIGNMENT.CENTER
        t.autofit = False
        self._borders(t)
        self._cell_margins(t)
        tblPr = t._tbl.tblPr
        layout = OxmlElement("w:tblLayout")
        layout.set(qn("w:type"), "fixed")
        tblPr.append(layout)
        # header repeat
        trPr = t.rows[0]._tr.get_or_add_trPr()
        th = OxmlElement("w:tblHeader")
        trPr.append(th)
        for r_i, row in enumerate([headers] + rows):
            for c_i, val in enumerate(row):
                cell = t.cell(r_i, c_i)
                cell.width = Inches(widths[c_i])
                p = cell.paragraphs[0]
                p.paragraph_format.space_after = Pt(0)
                p.paragraph_format.line_spacing = 1.05
                if c_i in center_cols:
                    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
                texts = str(val).split("\n")
                for k, tx in enumerate(texts):
                    if k:
                        p = cell.add_paragraph()
                        p.paragraph_format.space_after = Pt(0)
                        p.paragraph_format.line_spacing = 1.05
                        if c_i in center_cols:
                            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
                    if r_i == 0:
                        self._runs(p, tx, size=size, color="FFFFFF", bold=True, mono_size=size - 0.5)
                    else:
                        self._runs(p, tx, size=size, bold=(bold_first_col and c_i == 0), mono_size=size - 1)
                if r_i == 0:
                    self._cell_shade(cell, header_fill)
        # set grid widths
        grid = t._tbl.tblGrid
        for i, gc in enumerate(grid.findall(qn("w:gridCol"))):
            gc.set(qn("w:w"), str(int(widths[i] * 1440)))
        sp = self.doc.add_paragraph()
        sp.paragraph_format.space_after = Pt(6)
        sp.paragraph_format.line_spacing = 0.8
        return t

    # ------------------------------------------------------------- TOC
    def toc_placeholder(self):
        self._toc_anchor = self.doc.add_paragraph()
        return self._toc_anchor

    def build_toc(self, pages):
        """Inserts a real TOC field whose cached result lists the headings
        with the given page numbers (Word can refresh it with F9)."""
        anchor = self._toc_anchor._p
        entries = self.headings
        prev = anchor
        for i, (lvl, text, bm) in enumerate(entries):
            p = OxmlElement("w:p")
            pPr = OxmlElement("w:pPr")
            tabs = OxmlElement("w:tabs")
            tab = OxmlElement("w:tab")
            tab.set(qn("w:val"), "right")
            tab.set(qn("w:leader"), "dot")
            tab.set(qn("w:pos"), "9926")
            tabs.append(tab)
            pPr.append(tabs)
            sp = OxmlElement("w:spacing")
            sp.set(qn("w:after"), "30" if lvl == 1 else "10")
            sp.set(qn("w:before"), "50" if lvl == 1 else "0")
            pPr.append(sp)
            ind = OxmlElement("w:ind")
            ind.set(qn("w:left"), "0" if lvl == 1 else "360")
            pPr.append(ind)
            p.append(pPr)
            if i == 0:
                r = OxmlElement("w:r")
                f1 = OxmlElement("w:fldChar")
                f1.set(qn("w:fldCharType"), "begin")
                r.append(f1)
                it = OxmlElement("w:instrText")
                it.set(qn("xml:space"), "preserve")
                it.text = ' TOC \\h \\u \\z \\t "Heading 1,1,Heading 2,2," '
                r.append(it)
                f2 = OxmlElement("w:fldChar")
                f2.set(qn("w:fldCharType"), "separate")
                r.append(f2)
                p.append(r)
            hl = OxmlElement("w:hyperlink")
            hl.set(qn("w:anchor"), bm)
            for kind, val in (("t", text), ("tab", None), ("t", str(pages.get(bm, "")))):
                r = OxmlElement("w:r")
                rPr = OxmlElement("w:rPr")
                if lvl == 1:
                    b = OxmlElement("w:b")
                    rPr.append(b)
                sz = OxmlElement("w:sz")
                sz.set(qn("w:val"), "21" if lvl == 1 else "20")
                rPr.append(sz)
                r.append(rPr)
                if kind == "tab":
                    r.append(OxmlElement("w:tab"))
                else:
                    t = OxmlElement("w:t")
                    t.set(qn("xml:space"), "preserve")
                    t.text = val
                    r.append(t)
                hl.append(r)
            p.append(hl)
            if i == len(entries) - 1:
                r = OxmlElement("w:r")
                f3 = OxmlElement("w:fldChar")
                f3.set(qn("w:fldCharType"), "end")
                r.append(f3)
                p.append(r)
            prev.addnext(p)
            prev = p
        anchor.getparent().remove(anchor)

    def save(self, path):
        self.doc.save(path)
