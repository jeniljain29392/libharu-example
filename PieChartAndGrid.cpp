/*
* << Haru Free PDF Library 2.0.0 >> -- arc_demo.c
*
* Copyright (c) 1999-2006 Takeshi Kanno <takeshi_kanno@est.hi-ho.ne.jp>
*
* Permission to use, copy, modify, distribute and sell this software
* and its documentation for any purpose is hereby granted without fee,
* provided that the above copyright notice appear in all copies and
* that both that copyright notice and this permission notice appear
* in supporting documentation.
* It is provided "as is" without express or implied warranty.
*
*/

#include <iostream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <iomanip>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <setjmp.h>
#include "hpdf.h"
#include <direct.h>
#include <math.h>

jmp_buf env;

void error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void *user_data)
{
	printf("ERROR: error_no=%04X, detail_no=%u\n", (HPDF_UINT)error_no, (HPDF_UINT)detail_no);
	longjmp(env, 1);
}

#define MARGIN 50

class PieSlice {
public:
	PieSlice() {}

	PieSlice(int no_of_slices, HPDF_REAL label_pos_x, HPDF_REAL page_height)
	{
		slice_count = no_of_slices;
		label_pos.x = label_pos_x;
		increment = std::min(50.0, (page_height * 0.25 - MARGIN) / no_of_slices);
		label_pos.y = MARGIN + increment/4;
	}

	~PieSlice() {}

	void hex2RGB(int hexValue)
	{
		color.r = ((hexValue >> 16) & 0xFF) / 255.0;  // Extract the RR byte
		color.g = ((hexValue >> 8) & 0xFF) / 255.0;   // Extract the GG byte
		color.b = ((hexValue) & 0xFF) / 255.0;        // Extract the BB byte
	}

	void drawLabelHeader(HPDF_Page page)
	{
		label_pos.y += increment/2;

		HPDF_Page_SetRGBFill(page, 0, 0, 0);
		HPDF_Page_BeginText(page);
		HPDF_Page_MoveTextPos(page, label_pos.x + 60, label_pos.y - increment / 4);
		std::stringstream labelDesc;
		labelDesc << std::fixed << std::setprecision(3) << "min" << " - " << "max ";
		labelDesc << std::fixed << std::setprecision(2) << ",  Area : acres" << ",   in" << " %";
		HPDF_Page_ShowText(page, labelDesc.str().c_str());
		HPDF_Page_EndText(page);

		label_pos.y += increment;
	}

	void drawLabel(HPDF_Page page)
	{
		HPDF_Page_SetLineWidth(page, increment / 2);
		HPDF_Page_SetRGBStroke(page, color.r, color.g, color.b);
		HPDF_Page_SetLineCap(page, HPDF_BUTT_END);

		HPDF_Page_MoveTo(page, label_pos.x, label_pos.y);
		HPDF_Page_LineTo(page, label_pos.x + 40, label_pos.y);
		HPDF_Page_Stroke(page);

		HPDF_Page_SetRGBFill(page, 0, 0, 0);
		HPDF_Page_BeginText(page);
		HPDF_Page_MoveTextPos(page, label_pos.x + 60, label_pos.y - increment / 4);
		std::stringstream labelDesc;
		labelDesc << std::fixed << std::setprecision(3) << min_range << " - " << max_range;
		labelDesc << std::fixed << std::setprecision(2) << ",   Area : " << area << ",    " << percent << " %";
		HPDF_Page_ShowText(page, labelDesc.str().c_str());
		HPDF_Page_EndText(page);

		label_pos.y += increment;
	}

	int slice_count, increment;
	HPDF_Point label_pos;
	HPDF_RGBColor color;
	float area, percent, min_range, max_range;
};

class PieChart {
public:
	PieChart() {}

	PieChart(int outer_radius, int inner_radius, HPDF_REAL center_x, HPDF_REAL center_y)
	{
		center.x = center_x;
		center.y = center_y;
		_outer_radius = outer_radius;
		_inner_radius = inner_radius;
		pos.x = center_x;
		pos.y = center_y + outer_radius;
		slope_extension = inner_radius * 2.5;
		cummulative = 0;
	}
	
	~PieChart() {}

	void slopeExtension(HPDF_Point *arcPoint)
	{
		int sin_sign = arcPoint->y < 0 ? -1 : 1;
		int cos_sign = arcPoint->x < 0 ? -1 : 1;

		float angle = acosf(abs(arcPoint->x) / _outer_radius);
		arcPoint->x += slope_extension * cos(angle) * cos_sign;
		arcPoint->y += slope_extension * sin(angle) * sin_sign;
	}

	void drawArc(HPDF_Page page)
	{
		HPDF_Point arcHalfPoint;
		float arcStart = 360 * cummulative;
		float arcHalfArea = 360 * (cummulative + slice->percent * 0.5 / 100);
		float arcEnd = std::min(360.0f, 360 * (cummulative + slice->percent / 100));

		HPDF_Page_SetRGBFill(page, slice->color.r, slice->color.g, slice->color.b);
		HPDF_Page_MoveTo(page, center.x, center.y);
		HPDF_Page_LineTo(page, pos.x, pos.y);
		HPDF_Page_Arc(page, center.x, center.y, _outer_radius, arcStart, arcHalfArea);
		pos = HPDF_Page_GetCurrentPos(page);
		HPDF_Page_LineTo(page, center.x, center.y);
		HPDF_Page_Fill(page);

		arcHalfPoint.x = pos.x - center.x;
		arcHalfPoint.y = pos.y - center.y;
		slopeExtension(&arcHalfPoint);

		HPDF_Page_SetLineWidth(page, 1);
		HPDF_Page_SetRGBStroke(page, slice->color.r, slice->color.g, slice->color.b);
		HPDF_Page_MoveTo(page, center.x, center.y);
		HPDF_Page_LineTo(page, arcHalfPoint.x + center.x, arcHalfPoint.y + center.y);
		HPDF_Page_Stroke(page);

		if (slice->percent >= 5) {
			HPDF_Page_SetRGBFill(page, 0, 0, 0);
			HPDF_Page_BeginText(page);
			HPDF_Page_MoveTextPos(page, arcHalfPoint.x + center.x, arcHalfPoint.y + center.y);
			std::ostringstream percentage;
			percentage << " " << std::setprecision(3) << slice->percent << "%";
			HPDF_Page_ShowText(page, percentage.str().c_str());
			HPDF_Page_EndText(page);
		}

		HPDF_Page_SetRGBFill(page, slice->color.r, slice->color.g, slice->color.b);
		HPDF_Page_MoveTo(page, center.x, center.y);
		HPDF_Page_LineTo(page, pos.x, pos.y);
		HPDF_Page_Arc(page, center.x, center.y, _outer_radius, arcHalfArea, arcEnd);
		pos = HPDF_Page_GetCurrentPos(page);
		HPDF_Page_LineTo(page, center.x, center.y);
		HPDF_Page_Fill(page);

		cummulative += slice->percent / 100;

		slice->drawLabel(page);

		return;
	}

	HPDF_Point center, pos;
	float cummulative, _outer_radius, _inner_radius, slope_extension;
	PieSlice *slice;
};

class ExcelGrid {
public:
	ExcelGrid() {}

	ExcelGrid(HPDF_REAL pos_x, HPDF_REAL pos_y, int rows, int cols, HPDF_REAL grid_width) {
		pos.x = pos_x;
		pos.y = pos_y;
		_rows = rows;
		_cols = cols;
		cellWidth = grid_width / _cols;
	}

	~ExcelGrid() {}

	void drawGrid(HPDF_Page page)
	{
		HPDF_Page_SetLineWidth(page, 0.5);
		HPDF_Page_SetRGBStroke(page, 0, 0, 0);

		for (int i = 0; i <= _cols; i++) {
			HPDF_REAL x = i * cellWidth + pos.x;
			HPDF_Page_MoveTo(page, x, pos.y);
			HPDF_Page_LineTo(page, x, pos.y + cellHeight * _rows);
			HPDF_Page_Stroke(page);
		}

		for (int i = 0; i <= _rows; i++) {
			HPDF_REAL y = i * cellHeight + pos.y;
			HPDF_Page_MoveTo(page, pos.x, y);
			HPDF_Page_LineTo(page, pos.x + cellWidth * _cols, y);
			HPDF_Page_Stroke(page);
		}
	}

	void drawFields(HPDF_Page page, HPDF_Font font, HPDF_Font font_bold)
	{
		for (int i = 0; i < _rows; i++) {
			HPDF_REAL y = i * cellHeight + pos.y + 6;

			for (int j = 0; j < _cols; j++) {
				if (i == _rows - 1 || !j) HPDF_Page_SetFontAndSize(page, font_bold, 12);
				else HPDF_Page_SetFontAndSize(page, font, 12);

				HPDF_REAL x = j * cellWidth + pos.x + 6;
				HPDF_Page_BeginText(page);
				HPDF_Page_MoveTextPos(page, x, y);
				HPDF_Page_ShowText(page, fields[_rows - 1 - i][j].c_str());
				HPDF_Page_EndText(page);
			}
		}
	}

	HPDF_Point pos;
	HPDF_REAL cellHeight = 20, cellWidth;
	std::vector <std::vector <std::string>> fields;
	int _rows, _cols;
};

int main(int argc, char **argv)
{
	char fname[256];
	
	_getcwd(fname, 256);
	strcat(fname, ".pdf");

	HPDF_Doc pdf = HPDF_New(error_handler, NULL);
	if (!pdf) {
		printf("error: cannot create PdfDoc object\n");
		return 1;
	}

	if (setjmp(env)) {
		HPDF_Free(pdf);
		return 1;
	}

	/* add a new page object. */
	HPDF_Page page = HPDF_AddPage(pdf);

	HPDF_Font font = HPDF_GetFont(pdf, "Times-Roman", "WinAnsiEncoding");
	HPDF_Font font_bold = HPDF_GetFont(pdf, "Times-Bold", "WinAnsiEncoding");
	HPDF_Page_SetFontAndSize(page, font, 12);
	HPDF_REAL height = HPDF_Page_GetHeight(page);
	HPDF_REAL width = HPDF_Page_GetWidth(page);
	
	/* Draw Pie chart */
	int colormap_rgb[] = { 0xA16108, 0xBE832B, 0xD8A854, 0xF0CC85, 
		0xF0E58D, 0xE3ED94, 0xD2ED9D, 0xA9CE69, 0x80A933, 0x5A810C };
	HPDF_REAL area_percent[] = { 42.8, 13.1, 9.4, 7.7, 6.1, 4.9, 4.2, 3.9, 3.9, 4.1 };
	HPDF_REAL area_acres[] = { 7.45, 2.25, 1.64, 1.34, 1.07, 0.85, 0.73, 0.67, 0.68, 0.71 };
	HPDF_REAL levels[] = { 0.001, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1};

	HPDF_RGBColor color;
	PieChart *pie = new PieChart(50, 15, width / 4, height / 6);
	pie->slice = new PieSlice(10, width / 2, height);

	for (int i = 0; i < sizeof(levels) / sizeof(levels[0]) - 1; i++) {
		pie->slice->hex2RGB(colormap_rgb[i]);
		pie->slice->percent = area_percent[i];
		pie->slice->area = area_acres[i];
		pie->slice->min_range = levels[i];
		pie->slice->max_range = levels[i+1];
		pie->drawArc(page);
	}

	HPDF_Page_SetFontAndSize(page, font_bold, 14);
	pie->slice->drawLabelHeader(page);

	/* draw center circle of Pie chart */
	HPDF_Page_SetGrayStroke(page, 0);
	HPDF_Page_SetGrayFill(page, 1);
	HPDF_Page_Circle(page, pie->center.x, pie->center.y, pie->_inner_radius);
	HPDF_Page_Fill(page);

	/* Draw a location excel grid table */
	const int rows = 2, cols = 4;
	ExcelGrid *grid = new ExcelGrid(MARGIN / 2, height / 3.2, rows, cols, width-MARGIN);
	grid->drawGrid(page);

	std::vector <std::string> headers;
	headers.push_back("Location");
	headers.push_back("Upper-left corner");
	headers.push_back("Map center point");
	headers.push_back("Lower-right corner");
	grid->fields.push_back(headers);

	std::vector <std::string> values;
	values.push_back("longitude, latitiude (deg)");
	values.push_back("-88.027027 , 40.663761");
	values.push_back("-88.025756 , 40.658061");
	values.push_back("-88.024486, 40.652360");
	grid->fields.push_back(values);

	grid->drawFields(page, font, font_bold);

	/* save the document to a file */
	HPDF_SaveToFile(pdf, fname);

	/* clean up */
	delete pie;
	HPDF_Free(pdf);

	return 0;
}
