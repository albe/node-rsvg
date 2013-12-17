
#include "Rsvg.h"
#include <node_buffer.h>
#include <cairo-pdf.h>
#include <cairo-svg.h>
#include <cstring>
#include <string>
#include <cmath>

// Hack to ignore warning message. It's deprecated to include the rsvg-cairo.h
// file directly, but we need to do this in order to support older versions.
#define __RSVG_RSVG_H_INSIDE__
#include <librsvg/rsvg-cairo.h>
#undef __RSVG_RSVG_H_INSIDE__

// Support for old Cairo 1.8.8.
#ifndef CAIRO_FORMAT_INVALID
#define CAIRO_FORMAT_INVALID ((cairo_format_t) -1)
#endif

using namespace v8;

typedef enum {
	RENDER_FORMAT_INVALID = -1,
	RENDER_FORMAT_RAW = 0,
	RENDER_FORMAT_PNG = 1,
	RENDER_FORMAT_JPEG = 2,
	RENDER_FORMAT_PDF = 3,
	RENDER_FORMAT_SVG = 4,
	RENDER_FORMAT_VIPS = 5
} render_format_t;

static render_format_t RenderFormatFromString(const char* formatString) {
	if (!formatString) {
		return RENDER_FORMAT_INVALID;
	} else if (std::strcmp(formatString, "raw") == 0) {
		return RENDER_FORMAT_RAW;
	} else if (std::strcmp(formatString, "png") == 0) {
		return RENDER_FORMAT_PNG;
	} else if (std::strcmp(formatString, "jpeg") == 0) {
		return RENDER_FORMAT_JPEG;
	} else if (std::strcmp(formatString, "pdf") == 0) {
		return RENDER_FORMAT_PDF;
	} else if (std::strcmp(formatString, "svg") == 0) {
		return RENDER_FORMAT_SVG;
	} else if (std::strcmp(formatString, "vips") == 0) {
		return RENDER_FORMAT_VIPS;
	} else {
		return RENDER_FORMAT_INVALID;
	}
}

static Handle<Value> RenderFormatToString(render_format_t format) {
	const char* formatString =
		format == RENDER_FORMAT_RAW ? "raw" :
		format == RENDER_FORMAT_PNG ? "png" :
		format == RENDER_FORMAT_JPEG ? "jpeg" :
		format == RENDER_FORMAT_PDF ? "pdf" :
		format == RENDER_FORMAT_SVG ? "svg" :
		format == RENDER_FORMAT_VIPS ? "vips" :
		NULL;

	return formatString ? String::New(formatString) : Null();
}

static cairo_format_t CairoFormatFromString(const char* formatString) {
	if (!formatString) {
		return CAIRO_FORMAT_INVALID;
	} else if (std::strcmp(formatString, "argb32") == 0) {
		return CAIRO_FORMAT_ARGB32;
	} else if (std::strcmp(formatString, "rgb24") == 0) {
		return CAIRO_FORMAT_RGB24;
	} else if (std::strcmp(formatString, "a8") == 0) {
		return CAIRO_FORMAT_A8;
	} else if (std::strcmp(formatString, "a1") == 0) {
		return CAIRO_FORMAT_A1;
	} else if (std::strcmp(formatString, "rgb16_565") == 0) {
		return (cairo_format_t) CAIRO_FORMAT_RGB16_565;
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 12, 0)
	} else if (std::strcmp(formatString, "rgb30") == 0) {
		return CAIRO_FORMAT_RGB30;
#endif
	} else {
		return CAIRO_FORMAT_INVALID;
	}
}

static Handle<Value> CairoFormatToString(cairo_format_t format) {
	const char* formatString =
		format == CAIRO_FORMAT_ARGB32 ? "argb32" :
		format == CAIRO_FORMAT_RGB24 ? "rgb24" :
		format == CAIRO_FORMAT_A8 ? "a8" :
		format == CAIRO_FORMAT_A1 ? "a1" :
		format == CAIRO_FORMAT_RGB16_565 ? "rgb16_565" :
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 12, 0)
		format == CAIRO_FORMAT_RGB30 ? "rgb30" :
#endif
		NULL;

	return formatString ? String::New(formatString) : Null();
}

cairo_status_t GetDataChunks(void* closure, const unsigned char* chunk, unsigned int length) {
	std::string* data = reinterpret_cast<std::string*>(closure);
	data->append(reinterpret_cast<const char *>(chunk), length);
	return CAIRO_STATUS_SUCCESS;
}

Persistent<Function> Rsvg::constructor;

Rsvg::Rsvg(RsvgHandle* const handle) : _handle(handle) {}

Rsvg::~Rsvg() {
	g_object_unref(G_OBJECT(_handle));
}

void Rsvg::Init(Handle<Object> exports) {

#if !GLIB_CHECK_VERSION(2, 36, 0)
	// Initialize GObject types.
	g_type_init();
#endif

	// Prepare constructor template.
	Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
	tpl->SetClassName(String::NewSymbol("Rsvg"));
	tpl->InstanceTemplate()->SetInternalFieldCount(1);
	// Add methods to prototype.
	Local<ObjectTemplate> prototype = tpl->PrototypeTemplate();
	prototype->Set("getBaseURI", FunctionTemplate::New(GetBaseURI)->GetFunction());
	prototype->Set("setBaseURI", FunctionTemplate::New(SetBaseURI)->GetFunction());
	prototype->Set("getDPI", FunctionTemplate::New(GetDPI)->GetFunction());
	prototype->Set("setDPI", FunctionTemplate::New(SetDPI)->GetFunction());
	prototype->Set("getDPIX", FunctionTemplate::New(GetDPIX)->GetFunction());
	prototype->Set("setDPIX", FunctionTemplate::New(SetDPIX)->GetFunction());
	prototype->Set("getDPIY", FunctionTemplate::New(GetDPIY)->GetFunction());
	prototype->Set("setDPIY", FunctionTemplate::New(SetDPIY)->GetFunction());
	prototype->Set("getWidth", FunctionTemplate::New(GetWidth)->GetFunction());
	prototype->Set("getHeight", FunctionTemplate::New(GetHeight)->GetFunction());
	prototype->Set("write", FunctionTemplate::New(Write)->GetFunction());
	prototype->Set("close", FunctionTemplate::New(Close)->GetFunction());
	prototype->Set("dimensions", FunctionTemplate::New(Dimensions)->GetFunction());
	prototype->Set("hasElement", FunctionTemplate::New(HasElement)->GetFunction());
	prototype->Set("autocrop", FunctionTemplate::New(Autocrop)->GetFunction());
	prototype->Set("render", FunctionTemplate::New(Render)->GetFunction());
	// Export class.
	constructor = Persistent<Function>::New(tpl->GetFunction());
	exports->Set(String::New("Rsvg"), constructor);
}

Handle<Value> Rsvg::New(const Arguments& args) {
	HandleScope scope;

	if (args.IsConstructCall()) {
		// Invoked as constructor: `new Rsvg(...)`
		RsvgHandle* handle;
		if (node::Buffer::HasInstance(args[0])) {
			const guint8* buffer =
				reinterpret_cast<guint8*>(node::Buffer::Data(args[0]));
			gsize length = node::Buffer::Length(args[0]);

			GError* error = NULL;
			handle = rsvg_handle_new_from_data(buffer, length, &error);

			if (error) {
				ThrowException(Exception::Error(String::New(error->message)));
				g_error_free(error);
				return scope.Close(Undefined());
			}
		} else {
			handle = rsvg_handle_new();
		}
		// Error handling.
		if (!handle) {
			ThrowException(Exception::Error(String::New(
				"Unable to create RsvgHandle instance."
			)));
			return scope.Close(Undefined());
		}
		// Create object.
		Rsvg* obj = new Rsvg(handle);
		obj->Wrap(args.This());
		return scope.Close(args.This());
	} else {
		// Invoked as plain function `Rsvg(...)`, turn into construct call.
		const int argc = 1;
		Local<Value> argv[argc] = { args[0] };
		return scope.Close(constructor->NewInstance(argc, argv));
	}
}

Handle<Value> Rsvg::GetBaseURI(const Arguments& args) {
	return GetStringProperty(args, "base-uri");
}

Handle<Value> Rsvg::SetBaseURI(const Arguments& args) {
	return SetStringProperty(args, "base-uri");
}

Handle<Value> Rsvg::GetDPI(const Arguments& args) {
	HandleScope scope;
	Rsvg* obj = node::ObjectWrap::Unwrap<Rsvg>(args.This());
	gdouble dpiX = 0;
	gdouble dpiY = 0;
	g_object_get(
		G_OBJECT(obj->_handle),
		"dpi-x", &dpiX,
		"dpi-y", &dpiY,
		NULL
	);

	Handle<ObjectTemplate> dpi = ObjectTemplate::New();
	dpi->Set("x", Number::New(dpiX));
	dpi->Set("y", Number::New(dpiY));

	return scope.Close(dpi->NewInstance());
}

Handle<Value> Rsvg::SetDPI(const Arguments& args) {
	HandleScope scope;
	Rsvg* obj = node::ObjectWrap::Unwrap<Rsvg>(args.This());

	gdouble x = args[0]->NumberValue();
	if (std::isnan(x)) {
		x = 0;
	}

	gdouble y = x;
	if (args[1]->IsNumber()) {
		y = args[1]->NumberValue();
		if (std::isnan(y)) {
			y = 0;
		}
	}

	rsvg_handle_set_dpi_x_y(obj->_handle, x, y);
	return scope.Close(Undefined());
}

Handle<Value> Rsvg::GetDPIX(const Arguments& args) {
	return GetNumberProperty(args, "dpi-x");
}

Handle<Value> Rsvg::SetDPIX(const Arguments& args) {
	return SetNumberProperty(args, "dpi-x");
}

Handle<Value> Rsvg::GetDPIY(const Arguments& args) {
	return GetNumberProperty(args, "dpi-y");
}

Handle<Value> Rsvg::SetDPIY(const Arguments& args) {
	return SetNumberProperty(args, "dpi-y");
}

Handle<Value> Rsvg::GetWidth(const Arguments& args) {
	return GetIntegerProperty(args, "width");
}

Handle<Value> Rsvg::GetHeight(const Arguments& args) {
	return GetIntegerProperty(args, "height");
}

Handle<Value> Rsvg::Write(const Arguments& args) {
	HandleScope scope;
	Rsvg* obj = node::ObjectWrap::Unwrap<Rsvg>(args.This());
	if (node::Buffer::HasInstance(args[0])) {
		const guchar* buffer =
			reinterpret_cast<guchar*>(node::Buffer::Data(args[0]));
		gsize length = node::Buffer::Length(args[0]);

		GError* error = NULL;
		gboolean success = rsvg_handle_write(obj->_handle, buffer, length, &error);

		if (error) {
			ThrowException(Exception::Error(String::New(error->message)));
			g_error_free(error);
		} else if (!success) {
			ThrowException(Exception::Error(String::New("Failed to write data.")));
		}
	} else {
		ThrowException(Exception::TypeError(String::New("Invalid argument: buffer")));
	}
	return scope.Close(Undefined());
}

Handle<Value> Rsvg::Close(const Arguments& args) {
	HandleScope scope;
	Rsvg* obj = node::ObjectWrap::Unwrap<Rsvg>(args.This());

	GError* error = NULL;
	gboolean success = rsvg_handle_close(obj->_handle, &error);

	if (error) {
		ThrowException(Exception::Error(String::New(error->message)));
		g_error_free(error);
	} else if (!success) {
		ThrowException(Exception::Error(String::New("Failed to close.")));
	}
	return scope.Close(Undefined());
}

Handle<Value> Rsvg::Dimensions(const Arguments& args) {
	HandleScope scope;
	Rsvg* obj = node::ObjectWrap::Unwrap<Rsvg>(args.This());

	const char* id = NULL;
	String::Utf8Value idArg(args[0]);
	if (!(args[0]->IsUndefined() || args[0]->IsNull())) {
		id = *idArg;
		if (!id) {
			ThrowException(Exception::TypeError(String::New("Invalid argument: id")));
			return scope.Close(Undefined());
		}
	}

	RsvgPositionData _position = { 0, 0 };
	RsvgDimensionData _dimensions = { 0, 0, 0, 0 };

	gboolean hasPosition = rsvg_handle_get_position_sub(obj->_handle, &_position, id);
	gboolean hasDimensions = rsvg_handle_get_dimensions_sub(obj->_handle, &_dimensions, id);

	if (hasPosition || hasDimensions) {
		Handle<ObjectTemplate> dimensions = ObjectTemplate::New();
		if (hasPosition) {
			dimensions->Set("x", Integer::New(_position.x));
			dimensions->Set("y", Integer::New(_position.y));
		}
		if (hasDimensions) {
			dimensions->Set("width", Integer::New(_dimensions.width));
			dimensions->Set("height", Integer::New(_dimensions.height));
		}
		return scope.Close(dimensions->NewInstance());
	} else {
		return scope.Close(Null());
	}
}

Handle<Value> Rsvg::HasElement(const Arguments& args) {
	HandleScope scope;
	Rsvg* obj = node::ObjectWrap::Unwrap<Rsvg>(args.This());

	const char* id = NULL;
	String::Utf8Value idArg(args[0]);
	if (!(args[0]->IsUndefined() || args[0]->IsNull())) {
		id = *idArg;
		if (!id) {
			ThrowException(Exception::TypeError(String::New("Invalid argument: id")));
			return scope.Close(Undefined());
		}
	}

	gboolean exists = rsvg_handle_has_sub(obj->_handle, id);
	return scope.Close(Boolean::New(exists));
}

struct autocrop_region_t
{
	double top;
	double bottom;
	double left;
	double right;
};

static inline uint32_t pixel(uint8_t* data, int stride, int x, int y) {
	return *reinterpret_cast<uint32_t*>(data + stride * y + x * 4);
}

static inline double round3(double number) {
	return round(number * 1000) * 0.001;
}

const uint32_t INVALID_COLOR = 0x00DEAD00;
static uint32_t areaColor(uint8_t* data, int stride, int x0, int x1, int y0, int y1) {
	uint32_t color = pixel(data, stride, x0, y0);
	for (int x = x0; x <= x1; x++) {
		for (int y = y0; y <= y1; y++) {
			uint32_t current = pixel(data, stride, x, y);
			if (current != color) {
				return INVALID_COLOR;
			}
		}
	}
	return color;
}

static uint32_t findEdge(uint8_t* data, int stride, int width, int height, int xd, int yd) {
	int x0 = 0;
	int x1 = width - 1;
	int y0 = 0;
	int y1 = height - 1;

	if (xd > 0) {
		x1 = x0;
	} else if (xd < 0) {
		x0 = x1;
	} else if (yd > 0) {
		y1 = y0;
	} else if (yd < 0) {
		y0 = y1;
	} else {
		return 0;
	}

	x0 -= xd;
	x1 -= xd;
	y0 -= yd;
	y1 -= yd;
	uint32_t edge = INVALID_COLOR;
	while (true) {
		x0 += xd;
		x1 += xd;
		y0 += yd;
		y1 += yd;
		if (x0 < 0 || x1 < 0 || y0 < 0 || y1 < 0 ||
				x0 >= width || x1 >= width || y0 >= height || y1 >= height) {
			x0 -= xd;
			x1 -= xd;
			y0 -= yd;
			y1 -= yd;
			break;
		}
		uint32_t color = areaColor(data, stride, x0, x1, y0, y1);
		if (edge == INVALID_COLOR) {
			edge = color;
		}
		if (color == INVALID_COLOR || color != edge) break;
	}

	return (xd != 0) ? x0 : y0;
}

static bool AutocropRecursive(RsvgHandle* handle, autocrop_region_t* region, int direction) {
	const int width = 100;
	const int height = 100;

	if (region->bottom - region->top < 0.0001 ||
			region->right - region->left < 0.0001) {
		return true;
	}

	cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cairo_t* cr = cairo_create(surface);

	cairo_scale(cr, width / (region->right - region->left), height / (region->bottom - region->top));
	cairo_translate(cr, -region->left, -region->top);

	gboolean success = rsvg_handle_render_cairo(handle, cr);
	cairo_surface_flush(surface);

	cairo_status_t status = cairo_status(cr);
	if (status || !success) {
		cairo_destroy(cr);
		cairo_surface_destroy(surface);

		ThrowException(Exception::Error(String::New(
			status ? cairo_status_to_string(status) : "Failed to render image."
		)));
		return false;
	}

	uint8_t* data = cairo_image_surface_get_data(surface);
	int stride = cairo_image_surface_get_stride(surface);

	if (areaColor(data, stride, 0, width - 1, 0, height - 1) != INVALID_COLOR) {
		cairo_destroy(cr);
		cairo_surface_destroy(surface);
		return true;
	}

	autocrop_region_t sub;

	if (direction == 1) {
		int top = findEdge(data, stride, width, height, 0, 1);
		sub.top = top;
		sub.bottom = top + 1;
		sub.left = 0;
		sub.right = width;
		cairo_device_to_user(cr, &sub.left, &sub.top);
		cairo_device_to_user(cr, &sub.right, &sub.bottom);
		success = AutocropRecursive(handle, &sub, direction);
		region->top = sub.top;
	} else if (direction == 2) {
		int bottom = findEdge(data, stride, width, height, 0, -1) + 1;
		sub.top = bottom - 1;
		sub.bottom = bottom;
		sub.left = 0;
		sub.right = width;
		cairo_device_to_user(cr, &sub.left, &sub.top);
		cairo_device_to_user(cr, &sub.right, &sub.bottom);
		success = AutocropRecursive(handle, &sub, direction);
		region->bottom = sub.bottom;
	} else if (direction == 3) {
		int left = findEdge(data, stride, width, height, 1, 0);
		sub.top = 0;
		sub.bottom = height;
		sub.left = left;
		sub.right = left + 1;
		cairo_device_to_user(cr, &sub.left, &sub.top);
		cairo_device_to_user(cr, &sub.right, &sub.bottom);
		success = AutocropRecursive(handle, &sub, direction);
		region->left = sub.left;
	} else if (direction == 4) {
		int right = findEdge(data, stride, width, height, -1, 0) + 1;
		sub.top = 0;
		sub.bottom = height;
		sub.left = right - 1;
		sub.right = right;
		cairo_device_to_user(cr, &sub.left, &sub.top);
		cairo_device_to_user(cr, &sub.right, &sub.bottom);
		success = AutocropRecursive(handle, &sub, direction);
		region->right = sub.right;
	} else {
		success = false;
	}

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	return success;
}

Handle<Value> Rsvg::Autocrop(const Arguments& args) {
	HandleScope scope;
	Rsvg* obj = node::ObjectWrap::Unwrap<Rsvg>(args.This());

	RsvgDimensionData dimensions = { 0, 0, 0, 0 };
	rsvg_handle_get_dimensions(obj->_handle, &dimensions);
	autocrop_region_t area = { 0, dimensions.height, 0, dimensions.width };

	if (AutocropRecursive(obj->_handle, &area, 1) &&
			AutocropRecursive(obj->_handle, &area, 2) &&
			AutocropRecursive(obj->_handle, &area, 3) &&
			AutocropRecursive(obj->_handle, &area, 4)) {
		Handle<ObjectTemplate> dimensions = ObjectTemplate::New();
		dimensions->Set("x", Number::New(round3(area.left)));
		dimensions->Set("y", Number::New(round3(area.top)));
		dimensions->Set("width", Number::New(round3(area.right - area.left)));
		dimensions->Set("height", Number::New(round3(area.bottom - area.top)));
		return scope.Close(dimensions->NewInstance());
	} else {
		return scope.Close(Undefined());
	}
}

Handle<Value> Rsvg::Render(const Arguments& args) {
	HandleScope scope;
	Rsvg* obj = node::ObjectWrap::Unwrap<Rsvg>(args.This());

	int width = args[0]->Int32Value();
	int height = args[1]->Int32Value();

	if (width <= 0) {
		ThrowException(Exception::RangeError(String::New("Expected width > 0.")));
		return scope.Close(Undefined());
	}
	if (height <= 0) {
		ThrowException(Exception::RangeError(String::New("Expected height > 0.")));
		return scope.Close(Undefined());
	}

	String::Utf8Value formatArg(args[2]);
	const char* formatString = *formatArg;
	render_format_t renderFormat = RenderFormatFromString(formatString);
	cairo_format_t pixelFormat = CAIRO_FORMAT_INVALID;
	if (renderFormat == RENDER_FORMAT_RAW ||
			renderFormat == RENDER_FORMAT_PNG) {
		pixelFormat = CAIRO_FORMAT_ARGB32;
	} else if (renderFormat == RENDER_FORMAT_JPEG) {
		ThrowException(Exception::Error(String::New("Format not supported: JPEG")));
		return scope.Close(Undefined());
	} else if (
			renderFormat == RENDER_FORMAT_SVG ||
			renderFormat == RENDER_FORMAT_PDF) {
		pixelFormat = CAIRO_FORMAT_INVALID;
	} else if (renderFormat == RENDER_FORMAT_VIPS) {
		ThrowException(Exception::Error(String::New("Format not supported: VIPS")));
		return scope.Close(Undefined());
	} else {
		renderFormat = RENDER_FORMAT_RAW;
		pixelFormat = CairoFormatFromString(formatString);
		if (pixelFormat == CAIRO_FORMAT_INVALID) {
			ThrowException(Exception::RangeError(String::New("Invalid argument: format")));
			return scope.Close(Undefined());
		}
	}

	const char* id = NULL;
	String::Utf8Value idArg(args[3]);
	if (!(args[3]->IsUndefined() || args[3]->IsNull())) {
		id = *idArg;
		if (!id) {
			ThrowException(Exception::TypeError(String::New("Invalid argument: id")));
			return scope.Close(Undefined());
		}
		if (!rsvg_handle_has_sub(obj->_handle, id)) {
			ThrowException(Exception::RangeError(String::New(
				"SVG element with given id does not exists."
			)));
			return scope.Close(Undefined());
		}
	}

	RsvgPositionData position = { 0, 0 };
	RsvgDimensionData dimensions = { 0, 0, 0, 0 };

	if (!rsvg_handle_get_position_sub(obj->_handle, &position, id)) {
		ThrowException(Exception::Error(String::New(
			"Could not get position of SVG element with given id."
		)));
		return scope.Close(Undefined());
	}

	if (!rsvg_handle_get_dimensions_sub(obj->_handle, &dimensions, id)) {
		ThrowException(Exception::Error(String::New(
			"Could not get dimensions of SVG element or whole image."
		)));
		return scope.Close(Undefined());
	}
	if (dimensions.width <= 0 || dimensions.height <= 0) {
		ThrowException(Exception::Error(String::New(
			"Got invalid dimensions of SVG element or whole image."
		)));
		return scope.Close(Undefined());
	}

	std::string data;
	cairo_surface_t* surface;

	if (renderFormat == RENDER_FORMAT_SVG) {
		surface = cairo_svg_surface_create_for_stream(GetDataChunks, &data, width, height);
		cairo_svg_surface_restrict_to_version(surface, CAIRO_SVG_VERSION_1_1);
	} else if (renderFormat == RENDER_FORMAT_PDF) {
		surface = cairo_pdf_surface_create_for_stream(GetDataChunks, &data, width, height);
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 10, 0)
		cairo_pdf_surface_restrict_to_version(surface, CAIRO_PDF_VERSION_1_4);
#endif
	} else {
		surface = cairo_image_surface_create(pixelFormat, width, height);
	}

	cairo_t* cr = cairo_create(surface);
	// printf(
	// 	"%s: (%d, %d) %dx%d, render: %dx%d\n",
	// 	id ? id : "SVG",
	// 	position.x,
	// 	position.y,
	// 	dimensions.width,
	// 	dimensions.height,
	// 	width,
	// 	height
	// );
	double scaleX = double(width) / double(dimensions.width);
	double scaleY = double(height) / double(dimensions.height);
	// printf("scale=%.3fx%.3f\n", scaleX, scaleY);
	double scale = MIN(scaleX, scaleY);
	int bboxWidth = round(scale * dimensions.width);
	int bboxHeight = round(scale * dimensions.height);
	int bboxX = (width - bboxWidth) / 2;
	int bboxY = (height - bboxHeight) / 2;
	// printf("bbox=(%d, %d) %dx%d\n", bboxX, bboxY, bboxWidth, bboxHeight);
	cairo_translate(cr, bboxX, bboxY);
	cairo_scale(cr, scale, scale);
	cairo_translate(cr, -position.x, -position.y);

	gboolean success;
	if (id) {
		success = rsvg_handle_render_cairo_sub(obj->_handle, cr, id);
	} else {
		success = rsvg_handle_render_cairo(obj->_handle, cr);
	}

	cairo_status_t status = cairo_status(cr);
	if (status || !success) {
		cairo_destroy(cr);
		cairo_surface_destroy(surface);

		ThrowException(Exception::Error(String::New(
			status ? cairo_status_to_string(status) : "Failed to render image."
		)));
		return scope.Close(Undefined());
	}

	cairo_surface_flush(surface);

	int stride = -1;
	if (renderFormat == RENDER_FORMAT_RAW) {
		stride = cairo_image_surface_get_stride(surface);
		data.append(
			reinterpret_cast<char*>(cairo_image_surface_get_data(surface)),
			stride * height
		);
	} else if (renderFormat == RENDER_FORMAT_PNG) {
		cairo_surface_write_to_png_stream(surface, GetDataChunks, &data);
	}

	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	if (renderFormat == RENDER_FORMAT_RAW &&
			pixelFormat == CAIRO_FORMAT_ARGB32 &&
			stride != width * 4) {
		ThrowException(Exception::Error(String::New(
			"Rendered with invalid stride (byte size of row) for ARGB32 format."
		)));
		return scope.Close(Undefined());
	}

	Handle<ObjectTemplate> image = ObjectTemplate::New();
	if (renderFormat == RENDER_FORMAT_SVG) {
		image->Set("data", String::New(data.c_str()));
	} else {
		image->Set("data", node::Buffer::New(data.c_str(), data.length())->handle_);
	}

	image->Set("format", RenderFormatToString(renderFormat));
	if (pixelFormat != CAIRO_FORMAT_INVALID) {
		image->Set("pixelFormat", CairoFormatToString(pixelFormat));
	}
	image->Set("width", Integer::New(width));
	image->Set("height", Integer::New(height));
	if (stride != -1) {
		image->Set("stride", Integer::New(stride));
	}
	return scope.Close(image->NewInstance());
}

Handle<Value> Rsvg::GetStringProperty(const Arguments& args, const char* property) {
	HandleScope scope;
	Rsvg* obj = node::ObjectWrap::Unwrap<Rsvg>(args.This());
	gchar* value = NULL;
	g_object_get(G_OBJECT(obj->_handle), property, &value, NULL);
	Handle<Value> result(value ? String::New(value) : Null());
	if (value) {
		g_free(value);
	}
	return scope.Close(result);
}

Handle<Value> Rsvg::SetStringProperty(const Arguments& args, const char* property) {
	HandleScope scope;
	Rsvg* obj = node::ObjectWrap::Unwrap<Rsvg>(args.This());
	gchar* value = NULL;
	String::Utf8Value arg0(args[0]);
	if (!(args[0]->IsNull() || args[0]->IsUndefined())) {
		value = *arg0;
	}
	g_object_set(G_OBJECT(obj->_handle), property, value, NULL);
	return scope.Close(Undefined());
}

Handle<Value> Rsvg::GetNumberProperty(const Arguments& args, const char* property) {
	HandleScope scope;
	Rsvg* obj = node::ObjectWrap::Unwrap<Rsvg>(args.This());
	gdouble value = 0;
	g_object_get(G_OBJECT(obj->_handle), property, &value, NULL);
	return scope.Close(Number::New(value));
}

Handle<Value> Rsvg::SetNumberProperty(const Arguments& args, const char* property) {
	HandleScope scope;
	Rsvg* obj = node::ObjectWrap::Unwrap<Rsvg>(args.This());
	gdouble value = args[0]->NumberValue();
	if (std::isnan(value)) {
		value = 0;
	}
	g_object_set(G_OBJECT(obj->_handle), property, value, NULL);
	return scope.Close(Undefined());
}

Handle<Value> Rsvg::GetIntegerProperty(const Arguments& args, const char* property) {
	HandleScope scope;
	Rsvg* obj = node::ObjectWrap::Unwrap<Rsvg>(args.This());
	gint value = 0;
	g_object_get(G_OBJECT(obj->_handle), property, &value, NULL);
	return scope.Close(Integer::New(value));
}

Handle<Value> Rsvg::SetIntegerProperty(const Arguments& args, const char* property) {
	HandleScope scope;
	Rsvg* obj = node::ObjectWrap::Unwrap<Rsvg>(args.This());
	gint value = args[0]->Int32Value();
	g_object_set(G_OBJECT(obj->_handle), property, value, NULL);
	return scope.Close(Undefined());
}

NODE_MODULE(rsvg, Rsvg::Init)
