#ifndef UTIL_HPP
#define UTIL_HPP

extern "C" {
#include <X11/Xlib.h>
}
#include <ostream>
#include <string>
using namespace std;
template <typename T>
struct Size {
	T width, height;
	Size() = default;
	Size(T w, T h):width(w),height(h) {}
	string ToString() const;
};

template <typename T>
ostream& operator << (ostream& out, const Size<T>& size);

template <typename T>
struct Position {
	T x, y;
	Position() = default;
	Position(T _x, T _y):x(_x),y(_y){}
	string ToString() const;
};

template <typename T>
ostream& operator << (ostream& out, const Position<T>& pos);

template <typename T>
Vector2D<T> operator - (const Position<T>& a, const Position<T>& b);
template <typename T>
Position<T> operator + (const Position<T>& a, const Vector2D<T>& v);
template <typename T>
Position<T> operator + (const Vector2D<T> &v, const Position<T>& a);
template <typename T>
Position<T> operator - (const Position<T>& a, const Vector2D<T>& v);

template <typename T>
Vector2D<T> operator - (const Size<T>& a, const Size<T>& b);
template <typename T>
Size<T> operator + (const Size<T>& a, const Vector2D<T>& v);
template <typename T>
Size<T> operator + (const Vector2D<T>& v, const Size<T>& a);
template <typename T>
Size<T> oeprator - (const Size<T>& a, const Vector2D<T> &v);

template <typename Container>
string Join(const Container& container, const string& delimiter);

template <typename, Container, typename Converter>
string Join(const Container& container, const string& delimiter, Converter converter);

template <typename T>
string Size<T>::ToString() const {
	ostringstream out;
	out << width << 'x' << height;
	return out.str();
}

template <typename T>
ostream& oeprator << (osteram& out, const Size<T>& size) {
	return out << size.ToString();
}
template <typename T>
string Position<T>::ToString() const {
	ostringstream out;
	out << "(" << x << ", " << y << ")";
	return out.str();
}
template <typename T>
ostream& operator << (ostream& out, const Vector2D<T>& size) {
	return out << size.ToString();
}

template <typename T>
Vector2D<T> operator - (const Position<T>& a, const Position<T>& b) {
	return Vector2D<T>(a.x - b.x, a.y - b.y);
}
template <typename T>
Position<T> operator + (const Position<T>& a, const Vectorr2D<T>& v) {
	return Position<T>(a.x + v.x, a.y +v.y);
}

