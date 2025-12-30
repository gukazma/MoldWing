# -*- coding: utf-8 -*-
"""
Generate PDF documentation for MoldWing Real-time Texture Editing System
"""

from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import cm, mm
from reportlab.lib.colors import HexColor, black, white, gray
from reportlab.lib.enums import TA_CENTER, TA_LEFT, TA_JUSTIFY
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, PageBreak,
    Table, TableStyle, Image, ListFlowable, ListItem
)
from reportlab.lib import colors
from reportlab.graphics.shapes import Drawing, Rect, Line, String, Circle, Polygon
from reportlab.graphics import renderPDF

# Register Chinese fonts
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

# Try to register Chinese fonts (Windows)
import os
FONT_PATHS = [
    'C:/Windows/Fonts/msyh.ttc',      # Microsoft YaHei
    'C:/Windows/Fonts/simsun.ttc',    # SimSun
    'C:/Windows/Fonts/simhei.ttf',    # SimHei
]

CHINESE_FONT = 'Helvetica'  # Fallback
CHINESE_FONT_BOLD = 'Helvetica-Bold'

for font_path in FONT_PATHS:
    if os.path.exists(font_path):
        try:
            pdfmetrics.registerFont(TTFont('ChineseFont', font_path))
            CHINESE_FONT = 'ChineseFont'
            CHINESE_FONT_BOLD = 'ChineseFont'  # TTF may not have bold variant
            print(f"Registered Chinese font: {font_path}")
            break
        except Exception as e:
            print(f"Failed to register {font_path}: {e}")
            continue

# Page setup
PAGE_WIDTH, PAGE_HEIGHT = A4
MARGIN = 2 * cm

def create_styles():
    """Create custom paragraph styles"""
    styles = getSampleStyleSheet()

    # Title style
    styles.add(ParagraphStyle(
        name='DocTitle',
        parent=styles['Title'],
        fontName=CHINESE_FONT,
        fontSize=24,
        spaceAfter=30,
        textColor=HexColor('#1a365d'),
        alignment=TA_CENTER
    ))

    # Subtitle
    styles.add(ParagraphStyle(
        name='Subtitle',
        parent=styles['Normal'],
        fontName=CHINESE_FONT,
        fontSize=14,
        spaceAfter=20,
        textColor=HexColor('#4a5568'),
        alignment=TA_CENTER
    ))

    # Section header
    styles.add(ParagraphStyle(
        name='SectionHeader',
        parent=styles['Heading1'],
        fontName=CHINESE_FONT,
        fontSize=16,
        spaceBefore=20,
        spaceAfter=12,
        textColor=HexColor('#2c5282'),
        borderPadding=5
    ))

    # Subsection header
    styles.add(ParagraphStyle(
        name='SubsectionHeader',
        parent=styles['Heading2'],
        fontName=CHINESE_FONT,
        fontSize=13,
        spaceBefore=15,
        spaceAfter=8,
        textColor=HexColor('#2b6cb0')
    ))

    # Body text
    styles.add(ParagraphStyle(
        name='BodyTextCustom',
        parent=styles['Normal'],
        fontName=CHINESE_FONT,
        fontSize=10,
        spaceAfter=8,
        alignment=TA_JUSTIFY,
        leading=14
    ))

    # Code style
    styles.add(ParagraphStyle(
        name='CodeBlock',
        parent=styles['Normal'],
        fontName='Courier',
        fontSize=8,
        spaceAfter=8,
        leftIndent=10,
        backColor=HexColor('#f7fafc'),
        borderPadding=5,
        leading=11
    ))

    # Formula style
    styles.add(ParagraphStyle(
        name='FormulaBlock',
        parent=styles['Normal'],
        fontName='Courier',
        fontSize=10,
        spaceBefore=10,
        spaceAfter=10,
        alignment=TA_CENTER,
        textColor=HexColor('#2d3748')
    ))

    # Caption
    styles.add(ParagraphStyle(
        name='FigCaption',
        parent=styles['Normal'],
        fontName=CHINESE_FONT,
        fontSize=9,
        spaceAfter=15,
        alignment=TA_CENTER,
        textColor=HexColor('#718096')
    ))

    return styles

def create_flow_diagram():
    """Create a data flow diagram"""
    d = Drawing(450, 200)

    # Colors
    box_fill = HexColor('#e2e8f0')
    box_stroke = HexColor('#4a5568')
    arrow_color = HexColor('#3182ce')

    # Box positions
    boxes = [
        (20, 140, 80, 40, "Screen\nClick"),
        (120, 140, 80, 40, "Face\nPicker"),
        (220, 140, 80, 40, "Ray-Triangle\nIntersect"),
        (320, 140, 80, 40, "UV\nCoord"),
        (220, 60, 80, 40, "Edit\nBuffer"),
        (320, 60, 80, 40, "GPU\nTexture"),
    ]

    for x, y, w, h, label in boxes:
        d.add(Rect(x, y, w, h, fillColor=box_fill, strokeColor=box_stroke, strokeWidth=1))
        lines = label.split('\n')
        for i, line in enumerate(lines):
            d.add(String(x + w/2, y + h/2 + 5 - i*12, line,
                        fontSize=9, textAnchor='middle'))

    # Arrows
    arrows = [
        (100, 160, 120, 160),  # Screen -> FacePicker
        (200, 160, 220, 160),  # FacePicker -> Ray
        (300, 160, 320, 160),  # Ray -> UV
        (360, 140, 360, 100),  # UV -> down
        (320, 80, 300, 80),    # -> EditBuffer
        (300, 80, 320, 80),    # EditBuffer -> GPU
    ]

    for x1, y1, x2, y2 in arrows:
        d.add(Line(x1, y1, x2, y2, strokeColor=arrow_color, strokeWidth=2))
        # Arrow head
        if x2 > x1:  # Right arrow
            d.add(Polygon([x2, y2, x2-8, y2+4, x2-8, y2-4], fillColor=arrow_color))
        elif y2 < y1:  # Down arrow
            d.add(Polygon([x2, y2, x2-4, y2+8, x2+4, y2+8], fillColor=arrow_color))

    return d

def create_coordinate_diagram():
    """Create barycentric coordinate diagram"""
    d = Drawing(300, 180)

    # Triangle vertices
    v0 = (150, 150)
    v1 = (50, 30)
    v2 = (250, 30)

    # Triangle
    d.add(Polygon([v0[0], v0[1], v1[0], v1[1], v2[0], v2[1]],
                  fillColor=HexColor('#bee3f8'), strokeColor=HexColor('#2b6cb0'), strokeWidth=2))

    # Point P inside triangle
    px, py = 140, 70
    d.add(Circle(px, py, 5, fillColor=HexColor('#e53e3e'), strokeColor=black))

    # Vertex labels
    d.add(String(v0[0], v0[1]+10, "V0 (UV0)", fontSize=9, textAnchor='middle'))
    d.add(String(v1[0]-15, v1[1], "V1 (UV1)", fontSize=9, textAnchor='middle'))
    d.add(String(v2[0]+15, v2[1], "V2 (UV2)", fontSize=9, textAnchor='middle'))
    d.add(String(px+15, py, "P", fontSize=9, textAnchor='middle', fillColor=HexColor('#e53e3e')))

    return d

def build_document():
    """Build the complete PDF document"""
    doc = SimpleDocTemplate(
        "d:/codes/cpp/MoldWing/Docs/MoldWing-Texture-Edit-Technical-v2.pdf",
        pagesize=A4,
        leftMargin=MARGIN,
        rightMargin=MARGIN,
        topMargin=MARGIN,
        bottomMargin=MARGIN
    )

    styles = create_styles()
    story = []

    # ========== TITLE PAGE ==========
    story.append(Spacer(1, 3*cm))
    story.append(Paragraph("MoldWing 实时纹理编辑系统", styles['DocTitle']))
    story.append(Paragraph("技术实现文档", styles['Subtitle']))
    story.append(Spacer(1, 1*cm))
    story.append(Paragraph("Real-time Texture Editing Technical Documentation", styles['Subtitle']))
    story.append(Spacer(1, 2*cm))

    # Info table
    info_data = [
        ["项目名称", "MoldWing - 倾斜摄影三维模型编辑器"],
        ["文档版本", "1.0"],
        ["模块", "M7 - 克隆图章工具"],
        ["技术栈", "C++17, Qt 6, DiligentEngine"],
    ]
    info_table = Table(info_data, colWidths=[4*cm, 10*cm])
    info_table.setStyle(TableStyle([
        ('FONTNAME', (0, 0), (-1, -1), CHINESE_FONT),
        ('BACKGROUND', (0, 0), (0, -1), HexColor('#edf2f7')),
        ('TEXTCOLOR', (0, 0), (-1, -1), HexColor('#2d3748')),
        ('ALIGN', (0, 0), (-1, -1), 'LEFT'),
        ('FONTSIZE', (0, 0), (-1, -1), 10),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 10),
        ('TOPPADDING', (0, 0), (-1, -1), 10),
        ('GRID', (0, 0), (-1, -1), 0.5, HexColor('#cbd5e0')),
    ]))
    story.append(info_table)

    story.append(PageBreak())

    # ========== TABLE OF CONTENTS ==========
    story.append(Paragraph("目录", styles['SectionHeader']))
    toc_items = [
        "1. 系统概述",
        "2. 屏幕空间到纹理空间映射",
        "    2.1 GPU面拾取 (Face Picking)",
        "    2.2 射线-三角形求交 (Moller-Trumbore)",
        "    2.3 重心坐标计算",
        "    2.4 UV坐标插值",
        "3. CPU纹理编辑缓冲区",
        "    3.1 TextureEditBuffer 设计",
        "    3.2 脏区域追踪",
        "4. GPU纹理实时更新",
        "5. 克隆图章算法",
        "    5.1 源点选择",
        "    5.2 偏移计算",
        "    5.3 像素复制",
        "6. 撤销/重做系统",
        "7. 完整数据流程",
    ]
    for item in toc_items:
        indent = 20 if item.startswith("    ") else 0
        story.append(Paragraph(item.strip(),
                              ParagraphStyle('TOC', parent=styles['BodyTextCustom'], fontName=CHINESE_FONT, leftIndent=indent)))

    story.append(PageBreak())

    # ========== SECTION 1: OVERVIEW ==========
    story.append(Paragraph("1. 系统概述", styles['SectionHeader']))

    story.append(Paragraph(
        "MoldWing 纹理编辑系统采用<b>屏幕空间投影</b>方式进行纹理编辑，"
        "与传统的 UV 展开编辑不同，用户直接在 3D 视图上操作，系统自动将屏幕坐标映射到纹理坐标。"
        "这种方式具有以下优势：",
        styles['BodyTextCustom']
    ))

    advantages = [
        "所见即所得 (WYSIWYG) - 编辑效果实时反映在 3D 模型上",
        "无需理解 UV 布局 - 降低用户学习成本",
        "保持原生纹理分辨率 - 实际编辑在纹理空间进行",
        "支持撤销/重做 - 完整的编辑历史管理"
    ]
    for adv in advantages:
        story.append(Paragraph(f"• {adv}", styles['BodyTextCustom']))

    story.append(Spacer(1, 0.5*cm))
    story.append(Paragraph("<b>核心组件架构：</b>", styles['BodyTextCustom']))

    components_data = [
        ["组件", "职责", "关键类"],
        ["GPU拾取", "确定鼠标点击的三角形面", "FacePicker"],
        ["坐标映射", "屏幕坐标 → 纹理坐标", "ScreenToTextureMapper"],
        ["编辑缓冲", "CPU端纹理数据管理", "TextureEditBuffer"],
        ["GPU更新", "将编辑同步到GPU纹理", "MeshRenderer"],
        ["撤销系统", "像素级变更记录与恢复", "TextureEditCommand"],
    ]
    comp_table = Table(components_data, colWidths=[3*cm, 6*cm, 5*cm])
    comp_table.setStyle(TableStyle([
        ('FONTNAME', (0, 0), (-1, -1), CHINESE_FONT),
        ('BACKGROUND', (0, 0), (-1, 0), HexColor('#4a5568')),
        ('TEXTCOLOR', (0, 0), (-1, 0), white),
        ('ALIGN', (0, 0), (-1, -1), 'LEFT'),
        ('FONTSIZE', (0, 0), (-1, -1), 9),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 8),
        ('TOPPADDING', (0, 0), (-1, -1), 8),
        ('GRID', (0, 0), (-1, -1), 0.5, HexColor('#cbd5e0')),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [white, HexColor('#f7fafc')]),
    ]))
    story.append(comp_table)

    story.append(PageBreak())

    # ========== SECTION 2: SCREEN TO TEXTURE MAPPING ==========
    story.append(Paragraph("2. 屏幕空间到纹理空间映射", styles['SectionHeader']))

    story.append(Paragraph(
        "将屏幕上的鼠标点击位置转换为纹理坐标是整个系统的核心。这个过程分为四个步骤：",
        styles['BodyTextCustom']
    ))

    # Data flow diagram
    story.append(Spacer(1, 0.3*cm))
    story.append(create_flow_diagram())
    story.append(Paragraph("图1: 屏幕到纹理坐标的转换流程", styles['FigCaption']))

    # 2.1 Face Picking
    story.append(Paragraph("2.1 GPU面拾取 (Face Picking)", styles['SubsectionHeader']))
    story.append(Paragraph(
        "使用 GPU 渲染一个特殊的 ID 缓冲区，每个三角形面以其索引值作为颜色输出。"
        "通过读取鼠标位置的像素值，可以快速确定点击的是哪个三角形。",
        styles['BodyTextCustom']
    ))

    story.append(Paragraph("<b>Pixel Shader (HLSL):</b>", styles['BodyTextCustom']))
    story.append(Paragraph(
        "uint faceID = SV_PrimitiveID;  // Hardware-provided primitive ID<br/>"
        "output.Color = float4(<br/>"
        "    float(faceID &amp; 0xFF) / 255.0,<br/>"
        "    float((faceID >> 8) &amp; 0xFF) / 255.0,<br/>"
        "    float((faceID >> 16) &amp; 0xFF) / 255.0,<br/>"
        "    1.0);",
        styles['CodeBlock']
    ))

    # 2.2 Ray-Triangle Intersection
    story.append(Paragraph("2.2 射线-三角形求交 (Moller-Trumbore 算法)", styles['SubsectionHeader']))
    story.append(Paragraph(
        "获得面ID后，需要计算射线与该三角形的精确交点。Moller-Trumbore 算法是最高效的方法：",
        styles['BodyTextCustom']
    ))

    story.append(Paragraph(
        "射线定义: R(t) = O + t·D<br/>"
        "三角形定义: P = (1-u-v)·V0 + u·V1 + v·V2",
        styles['FormulaBlock']
    ))

    story.append(Paragraph("<b>算法步骤：</b>", styles['BodyTextCustom']))
    algo_steps = [
        "1. 计算边向量: E1 = V1 - V0, E2 = V2 - V0",
        "2. 计算叉积: P = D × E2",
        "3. 计算行列式: det = E1 · P",
        "4. 如果 |det| < ε，射线与三角形平行，无交点",
        "5. 计算 T = O - V0",
        "6. 计算重心坐标 u = (T · P) / det",
        "7. 计算 Q = T × E1",
        "8. 计算重心坐标 v = (D · Q) / det",
        "9. 计算距离 t = (E2 · Q) / det",
        "10. 验证: u ≥ 0, v ≥ 0, u + v ≤ 1, t > 0"
    ]
    for step in algo_steps:
        story.append(Paragraph(step, ParagraphStyle('Step', parent=styles['BodyTextCustom'], fontName=CHINESE_FONT, leftIndent=15)))

    story.append(PageBreak())

    # 2.3 Barycentric Coordinates
    story.append(Paragraph("2.3 重心坐标计算", styles['SubsectionHeader']))

    story.append(create_coordinate_diagram())
    story.append(Paragraph("图2: 三角形内任意点P的重心坐标表示", styles['FigCaption']))

    story.append(Paragraph(
        "重心坐标 (u, v, w) 满足: <b>P = w·V0 + u·V1 + v·V2</b>，其中 w = 1 - u - v。"
        "这些坐标表示点P到各顶点的相对权重，可用于插值任何顶点属性。",
        styles['BodyTextCustom']
    ))

    # 2.4 UV Interpolation
    story.append(Paragraph("2.4 UV坐标插值", styles['SubsectionHeader']))
    story.append(Paragraph(
        "使用重心坐标对三个顶点的UV值进行线性插值，得到交点的精确UV坐标：",
        styles['BodyTextCustom']
    ))

    story.append(Paragraph(
        "UV_P = w·UV0 + u·UV1 + v·UV2<br/><br/>"
        "texX = (int)(UV_P.u × textureWidth)<br/>"
        "texY = (int)((1 - UV_P.v) × textureHeight)  // V轴翻转",
        styles['FormulaBlock']
    ))

    story.append(Paragraph("<b>C++ 实现 (ScreenToTextureMapper.cpp):</b>", styles['BodyTextCustom']))
    story.append(Paragraph(
        "void ScreenToTextureMapper::interpolateUV(<br/>"
        "    const float* uv0, const float* uv1, const float* uv2,<br/>"
        "    float baryU, float baryV,<br/>"
        "    float&amp; outU, float&amp; outV)<br/>"
        "{<br/>"
        "    float baryW = 1.0f - baryU - baryV;<br/>"
        "    outU = baryW * uv0[0] + baryU * uv1[0] + baryV * uv2[0];<br/>"
        "    outV = baryW * uv0[1] + baryU * uv1[1] + baryV * uv2[1];<br/>"
        "}",
        styles['CodeBlock']
    ))

    story.append(PageBreak())

    # ========== SECTION 3: TEXTURE EDIT BUFFER ==========
    story.append(Paragraph("3. CPU纹理编辑缓冲区", styles['SectionHeader']))

    story.append(Paragraph("3.1 TextureEditBuffer 设计", styles['SubsectionHeader']))
    story.append(Paragraph(
        "TextureEditBuffer 是纹理编辑的核心数据结构，它在 CPU 端维护一份纹理的可编辑副本：",
        styles['BodyTextCustom']
    ))

    buffer_struct = [
        ["成员", "类型", "说明"],
        ["m_data", "vector<uint8_t>", "当前编辑数据 (RGBA)"],
        ["m_originalData", "vector<uint8_t>", "原始备份 (用于撤销/橡皮擦)"],
        ["m_dirtyRects", "vector<QRect>", "需要GPU同步的脏区域"],
        ["m_width, m_height", "int", "纹理尺寸"],
        ["m_modified", "bool", "是否有未保存修改"],
    ]
    buf_table = Table(buffer_struct, colWidths=[3.5*cm, 4*cm, 6.5*cm])
    buf_table.setStyle(TableStyle([
        ('FONTNAME', (0, 0), (-1, -1), CHINESE_FONT),
        ('BACKGROUND', (0, 0), (-1, 0), HexColor('#4a5568')),
        ('TEXTCOLOR', (0, 0), (-1, 0), white),
        ('FONTNAME', (0, 1), (0, -1), 'Courier'),
        ('FONTNAME', (1, 1), (1, -1), 'Courier'),
        ('FONTSIZE', (0, 0), (-1, -1), 9),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 6),
        ('TOPPADDING', (0, 0), (-1, -1), 6),
        ('GRID', (0, 0), (-1, -1), 0.5, HexColor('#cbd5e0')),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [white, HexColor('#f7fafc')]),
    ]))
    story.append(buf_table)

    story.append(Spacer(1, 0.3*cm))
    story.append(Paragraph("<b>关键API：</b>", styles['BodyTextCustom']))
    story.append(Paragraph(
        "// Pixel read/write<br/>"
        "void setPixel(int x, int y, uint8_t r, g, b, a);<br/>"
        "void getPixel(int x, int y, uint8_t&amp; r, g, b, a) const;<br/>"
        "void getOriginalPixel(int x, int y, ...) const;  // Read original value<br/><br/>"
        "// Dirty region management<br/>"
        "void markDirty(const QRect&amp; rect);<br/>"
        "bool isDirty() const;<br/>"
        "void clearDirty();",
        styles['CodeBlock']
    ))

    story.append(Paragraph("3.2 脏区域追踪", styles['SubsectionHeader']))
    story.append(Paragraph(
        "为了优化GPU更新性能，系统追踪哪些区域被修改过。只有脏区域需要上传到GPU："
        "<br/><br/>"
        "• 每次 setPixel() 调用会标记对应像素的包围盒为脏<br/>"
        "• 多个脏区域可以合并为一个包围矩形 (dirtyBounds())<br/>"
        "• GPU更新后调用 clearDirty() 重置状态",
        styles['BodyTextCustom']
    ))

    story.append(PageBreak())

    # ========== SECTION 4: GPU UPDATE ==========
    story.append(Paragraph("4. GPU纹理实时更新", styles['SectionHeader']))

    story.append(Paragraph(
        "当 CPU 端的编辑缓冲区被修改后，需要将变更同步到 GPU 纹理才能在渲染中显示。"
        "DiligentEngine 提供了高效的纹理更新接口：",
        styles['BodyTextCustom']
    ))

    story.append(Paragraph("<b>MeshRenderer::updateTextureFromBuffer():</b>", styles['BodyTextCustom']))
    story.append(Paragraph(
        "void MeshRenderer::updateTextureFromBuffer(<br/>"
        "    IDeviceContext* pContext,<br/>"
        "    int textureIndex,<br/>"
        "    const TextureEditBuffer&amp; buffer)<br/>"
        "{<br/>"
        "    if (!buffer.isDirty()) return;<br/><br/>"
        "    // Prepare texture data<br/>"
        "    TextureSubResData subResData;<br/>"
        "    subResData.pData = buffer.data();<br/>"
        "    subResData.Stride = buffer.bytesPerLine();<br/><br/>"
        "    // Calculate update region<br/>"
        "    Box updateBox;<br/>"
        "    QRect dirtyBounds = buffer.dirtyBounds();<br/>"
        "    updateBox.MinX = dirtyBounds.left();<br/>"
        "    updateBox.MinY = dirtyBounds.top();<br/>"
        "    updateBox.MaxX = dirtyBounds.right() + 1;<br/>"
        "    updateBox.MaxY = dirtyBounds.bottom() + 1;<br/><br/>"
        "    // Upload to GPU<br/>"
        "    pContext->UpdateTexture(<br/>"
        "        m_textures[textureIndex],<br/>"
        "        0, 0,  // mip level, array slice<br/>"
        "        updateBox,<br/>"
        "        subResData,<br/>"
        "        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);<br/>"
        "}",
        styles['CodeBlock']
    ))

    story.append(Paragraph(
        "<b>性能优化：</b> 通过只更新脏区域而非整个纹理，可以显著减少 CPU-GPU 数据传输量。"
        "对于 4K 纹理 (4096×4096×4 = 64MB)，一次小型笔刷编辑可能只需要传输几KB数据。",
        styles['BodyTextCustom']
    ))

    story.append(PageBreak())

    # ========== SECTION 5: CLONE STAMP ==========
    story.append(Paragraph("5. 克隆图章算法", styles['SectionHeader']))

    story.append(Paragraph(
        "克隆图章工具允许用户从纹理的一个位置复制像素到另一个位置，实现纹理修复和复制效果。",
        styles['BodyTextCustom']
    ))

    story.append(Paragraph("5.1 源点选择", styles['SubsectionHeader']))
    story.append(Paragraph(
        "用户通过 <b>Alt + 左键点击</b> 设置克隆源位置。系统记录源点的纹理坐标：",
        styles['BodyTextCustom']
    ))
    story.append(Paragraph(
        "// DiligentWidget.cpp - Alt+Click handling<br/>"
        "if (m_interactionMode == InteractionMode::TextureEdit)<br/>"
        "{<br/>"
        "    m_cloneSourceTexX = result.texX;<br/>"
        "    m_cloneSourceTexY = result.texY;<br/>"
        "    m_cloneSourceSet = true;<br/>"
        "    m_cloneFirstDestTexX = -1;  // Reset first dest point<br/>"
        "    m_cloneFirstDestTexY = -1;<br/>"
        "}",
        styles['CodeBlock']
    ))

    story.append(Paragraph("5.2 偏移计算", styles['SubsectionHeader']))
    story.append(Paragraph(
        "首次绘制时，记录目标起始点，计算源与目标的固定偏移：",
        styles['BodyTextCustom']
    ))
    story.append(Paragraph(
        "offset = source_position - first_destination_position<br/><br/>"
        "// Subsequent painting:<br/>"
        "sample_position = current_destination + offset",
        styles['FormulaBlock']
    ))

    story.append(Paragraph(
        "这样在拖拽过程中，源采样位置会跟随目标位置移动，保持相对位置不变。",
        styles['BodyTextCustom']
    ))

    story.append(Paragraph("5.3 像素复制", styles['SubsectionHeader']))
    story.append(Paragraph("<b>paintBrushAtPosition() 核心逻辑：</b>", styles['BodyTextCustom']))
    story.append(Paragraph(
        "void DiligentWidget::paintBrushAtPosition(int texX, int texY)<br/>"
        "{<br/>"
        "    // Calculate offset<br/>"
        "    if (m_cloneFirstDestTexX < 0) {<br/>"
        "        m_cloneFirstDestTexX = texX;<br/>"
        "        m_cloneFirstDestTexY = texY;<br/>"
        "    }<br/>"
        "    int offsetX = m_cloneSourceTexX - m_cloneFirstDestTexX;<br/>"
        "    int offsetY = m_cloneSourceTexY - m_cloneFirstDestTexY;<br/><br/>"
        "    // Copy pixels within circular brush<br/>"
        "    for (int dy = -radius; dy <= radius; ++dy) {<br/>"
        "        for (int dx = -radius; dx <= radius; ++dx) {<br/>"
        "            if (dx*dx + dy*dy > radius*radius) continue;<br/><br/>"
        "            int destX = texX + dx, destY = texY + dy;<br/>"
        "            int srcX = destX + offsetX, srcY = destY + offsetY;<br/><br/>"
        "            // Read source pixel from original texture<br/>"
        "            m_editBuffer->getOriginalPixel(srcX, srcY, r, g, b, a);<br/>"
        "            // Write to destination<br/>"
        "            m_editBuffer->setPixel(destX, destY, r, g, b, a);<br/>"
        "        }<br/>"
        "    }<br/>"
        "}",
        styles['CodeBlock']
    ))

    story.append(PageBreak())

    # ========== SECTION 6: UNDO/REDO ==========
    story.append(Paragraph("6. 撤销/重做系统", styles['SectionHeader']))

    story.append(Paragraph(
        "系统使用 Qt 的 QUndoStack 框架实现撤销/重做功能。TextureEditCommand 记录每次笔刷操作"
        "涉及的所有像素变更：",
        styles['BodyTextCustom']
    ))

    story.append(Paragraph("<b>PixelChange 结构：</b>", styles['BodyTextCustom']))
    story.append(Paragraph(
        "struct PixelChange {<br/>"
        "    int16_t x, y;           // Pixel position<br/>"
        "    uint8_t oldR, oldG, oldB, oldA;  // Old values<br/>"
        "    uint8_t newR, newG, newB, newA;  // New values<br/>"
        "};",
        styles['CodeBlock']
    ))

    story.append(Paragraph("<b>工作流程：</b>", styles['BodyTextCustom']))
    undo_steps = [
        ["阶段", "操作", "说明"],
        ["beginPaint", "创建 TextureEditCommand", "准备记录变更"],
        ["paint", "recordPixel(x,y,old,new)", "记录每个像素的前后值"],
        ["endPaint", "finalize() + push()", "完成命令并压入撤销栈"],
        ["Ctrl+Z", "undo()", "恢复所有像素到旧值"],
        ["Ctrl+Y", "redo()", "重新应用所有像素新值"],
    ]
    undo_table = Table(undo_steps, colWidths=[3*cm, 5*cm, 6*cm])
    undo_table.setStyle(TableStyle([
        ('FONTNAME', (0, 0), (-1, -1), CHINESE_FONT),
        ('BACKGROUND', (0, 0), (-1, 0), HexColor('#4a5568')),
        ('TEXTCOLOR', (0, 0), (-1, 0), white),
        ('FONTNAME', (1, 1), (1, -1), 'Courier'),
        ('FONTSIZE', (0, 0), (-1, -1), 9),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 6),
        ('TOPPADDING', (0, 0), (-1, -1), 6),
        ('GRID', (0, 0), (-1, -1), 0.5, HexColor('#cbd5e0')),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [white, HexColor('#f7fafc')]),
    ]))
    story.append(undo_table)

    story.append(Spacer(1, 0.3*cm))
    story.append(Paragraph(
        "<b>GPU同步：</b> 撤销/重做操作会修改 CPU 缓冲区并标记脏区域。通过连接 QUndoStack::indexChanged "
        "信号到 syncTextureToGPU() 槽，确保每次撤销/重做后 GPU 纹理自动更新。",
        styles['BodyTextCustom']
    ))

    story.append(Paragraph(
        "connect(m_undoStack, &amp;QUndoStack::indexChanged,<br/>"
        "        this, [this]() { syncTextureToGPU(); });",
        styles['CodeBlock']
    ))

    story.append(PageBreak())

    # ========== SECTION 7: COMPLETE FLOW ==========
    story.append(Paragraph("7. 完整数据流程", styles['SectionHeader']))

    story.append(Paragraph(
        "以下是一次完整的克隆图章操作的数据流程：",
        styles['BodyTextCustom']
    ))

    flow_data = [
        ["步骤", "组件", "输入", "输出"],
        ["1", "鼠标事件", "屏幕坐标 (x, y)", "触发处理"],
        ["2", "FacePicker", "屏幕坐标", "Face ID"],
        ["3", "ScreenToTextureMapper", "Face ID + 屏幕坐标", "UV坐标"],
        ["4", "UV → Pixel", "UV + 纹理尺寸", "纹理像素坐标"],
        ["5", "paintBrushAtPosition", "目标坐标 + 偏移", "修改 EditBuffer"],
        ["6", "TextureEditCommand", "像素变更", "记录撤销数据"],
        ["7", "updateTextureFromBuffer", "脏区域数据", "GPU纹理更新"],
        ["8", "渲染循环", "更新后的纹理", "屏幕显示"],
    ]
    flow_table = Table(flow_data, colWidths=[1.5*cm, 4.5*cm, 4*cm, 4*cm])
    flow_table.setStyle(TableStyle([
        ('FONTNAME', (0, 0), (-1, -1), CHINESE_FONT),
        ('BACKGROUND', (0, 0), (-1, 0), HexColor('#2c5282')),
        ('TEXTCOLOR', (0, 0), (-1, 0), white),
        ('FONTSIZE', (0, 0), (-1, -1), 9),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 8),
        ('TOPPADDING', (0, 0), (-1, -1), 8),
        ('GRID', (0, 0), (-1, -1), 0.5, HexColor('#cbd5e0')),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [white, HexColor('#ebf8ff')]),
        ('ALIGN', (0, 0), (0, -1), 'CENTER'),
    ]))
    story.append(flow_table)

    story.append(Spacer(1, 0.5*cm))
    story.append(Paragraph("<b>关键性能指标：</b>", styles['BodyTextCustom']))
    perf_data = [
        ["操作", "典型耗时", "瓶颈"],
        ["Face Picking (GPU)", "< 1ms", "GPU渲染"],
        ["Ray-Triangle 求交 (CPU)", "< 0.1ms", "数学计算"],
        ["像素编辑 (CPU)", "< 1ms", "内存带宽"],
        ["GPU纹理更新", "1-5ms", "PCIe传输"],
        ["总延迟", "< 16ms", "保持60FPS交互"],
    ]
    perf_table = Table(perf_data, colWidths=[5*cm, 3*cm, 6*cm])
    perf_table.setStyle(TableStyle([
        ('FONTNAME', (0, 0), (-1, -1), CHINESE_FONT),
        ('BACKGROUND', (0, 0), (-1, 0), HexColor('#4a5568')),
        ('TEXTCOLOR', (0, 0), (-1, 0), white),
        ('FONTSIZE', (0, 0), (-1, -1), 9),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 6),
        ('TOPPADDING', (0, 0), (-1, -1), 6),
        ('GRID', (0, 0), (-1, -1), 0.5, HexColor('#cbd5e0')),
    ]))
    story.append(perf_table)

    story.append(Spacer(1, 1*cm))
    story.append(Paragraph(
        "文档生成时间: 2025-12-30 | MoldWing Project",
        ParagraphStyle('Footer', parent=styles['Normal'],
                      fontName=CHINESE_FONT, fontSize=8, textColor=gray, alignment=TA_CENTER)
    ))

    # Build PDF
    doc.build(story)
    print("PDF generated: d:/codes/cpp/MoldWing/Docs/MoldWing-Texture-Edit-Technical-v2.pdf")

if __name__ == "__main__":
    build_document()
