/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    Eric Moyer,
    anonymous contributors,

  to the public domain (http://creativecommons.org/publicdomain/zero/1.0/).

  Note that some moral obligations still exist in the absence of legal ones.
  For example, it would still be dishonest to deliberately misrepresent the
  origin of a work. Although we impose no legal requirements to obtain a
  license, it is beseeming for those who build on the works of others to
  give back useful improvements, or find a way to pay it forward. If
  you would like to cite us, a published paper about Waffles can be found
  at http://jmlr.org/papers/volume12/gashler11a/gashler11a.pdf. If you find
  our code to be useful, the Waffles team would love to hear how you use it.
*/

#ifndef __GRECT_H__
#define __GRECT_H__

#include "WError.h"

namespace WClasses
{

/// Represents a rectangular region with integers
class WRect
{
public:
	int x, y, w, h;

	WRect()
	{
	}

	WRect(int _x, int _y, int _w, int _h)
	{
		x = _x;
		y = _y;
		w = _w;
		h = _h;
	}

	/// Sets all 4 values in this rect
	void set(int _x, int _y, int _w, int _h)
	{
		x = _x;
		y = _y;
		w = _w;
		h = _h;
	}

	/// Returns true of this rect includes the specified point
	bool doesInclude(int _x, int _y)
	{
		if(_x >= x && _y >= y && _x < x + w && _y < y + h)
			return true;
		else
			return false;
	}

	/// Clips this rect to fit within pClippingRect
	void clip(WRect* pClippingRect);
};

/// Represents a rectangular region with floats
class WFloatRect
{
public:
	float x, y, w, h;

	WFloatRect()
	{
	}

	WFloatRect(float _x, float _y, float _w, float _h)
	{
		x = _x;
		y = _y;
		w = _w;
		h = _h;
	}

	/// Sets all 4 values in this rect
	void set(float _x, float _y, float _w, float _h)
	{
		x = _x;
		y = _y;
		w = _w;
		h = _h;
	}

	/// Returns true iff this rect includes the specified point
	bool doesInclude(float _x, float _y)
	{
		if(_x >= x && _y >= y && _x < x + w && _y < y + h)
			return true;
		else
			return false;
	}
};

/// Represents a rectangular region with doubles
class WDoubleRect
{
public:
	double x, y, w, h;

	WDoubleRect()
	{
	}

	WDoubleRect(double _x, double _y, double _w, double _h)
	{
		x = _x;
		y = _y;
		w = _w;
		h = _h;
	}

	/// Sets all 4 values of this rect
	void set(double _x, double _y, double _w, double _h)
	{
		x = _x;
		y = _y;
		w = _w;
		h = _h;
	}

	/// Returns true if the specified point is within this rect
	bool doesInclude(double _x, double _y)
	{
		if(_x >= x && _y >= y && _x < x + w && _y < y + h)
			return true;
		else
			return false;
	}

	/// Increases the rect size to include the specified point
	void include(double _x, double _y)
	{
		if(_x < x)
		{
			w += (x - _x);
			x = _x;
		}
		else
			w = std::max(w, _x - x);
		if(_y < y)
		{
			h += (y - _y);
			y = _y;
		}
		else
			h = std::max(h, _y - y);
	}

	/// Minimally increases the size of this rect (equally on both sides)
	/// in order to obtain the specified aspect ratio.
	void makeAspect(double _w, double _h)
	{
		if(w / h < _w / _h)
		{
			double d = h * _w / _h - w;
			w += d;
			x -= d / 2;
		}
		else
		{
			double d = w * _h / _w - h;
			h += d;
			y -= d / 2;
		}
	}
};

} // namespace WClasses

#endif // __GRECT_H__
