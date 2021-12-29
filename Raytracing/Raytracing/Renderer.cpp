#include <chrono>

#include "Renderer.h"
#include "Sphere.h"
#include "HittableList.h"

using std::chrono::high_resolution_clock;
using std::chrono::duration;

RESULT Renderer::Init()
{
	// Get Desktop Device Context
	m_device_context = GetDC(0);

	// Allocate frame buffer
	m_frame_buffer = (COLORREF*) calloc(m_image_width * m_image_height, sizeof(COLORREF));
	CHECK_ERROR(m_frame_buffer != nullptr, "Failed to allocate memory for frame buffer.");

	return RESULT::RES_OK;
}

inline color ray_color(const Ray& r, const HittableList& world, int bounces_remaining)
{
	hit_record rec;

	if (bounces_remaining <= 0)
	{
		return color(0, 0, 0);
	}
	else if (world.hit(r, 0.001, infinity, rec))
	{
		point3 target = rec.p + rec.normal + random_unit_vector();
		return 0.5 * ray_color(Ray(rec.p, target - rec.p), world, bounces_remaining - 1);
	}

	// linear interpolation
	vec3 unit_direction = unit_vector(r.get_direction());
	double t = 0.5 * (unit_direction.y() + 1.0);
	return (1.0 - t) * color(1.0, 1.0, 1.0) + t * color(0.5, 0.7, 1.0);
}

void Renderer::CastRays()
{
	// Timing
	std::chrono::steady_clock::time_point start_time = high_resolution_clock::now();

	// Create the objects in the world
	HittableList world;
	world.add(make_shared<Sphere>(point3(0, 0, -1), 0.5));
	world.add(make_shared<Sphere>(point3(0, -100.5, -1), 100)); // the ground

	// Fill the bitmap
	for (int j = m_image_height - 1; j >= 0; j--)
	{
		// Show progress
		std::cerr << "\rScanlines remaining: " << (m_image_height - j) << ' ' << "/ " << m_image_height << "\t\t\t\t" << std::flush;

		for (int i = 0; i < m_image_width; i++)
		{
			color pixel_color(0, 0, 0);
			for (int k = 0; k < m_sample_size_per_pixel; k++)
			{

				double u = double(i) / (m_image_width - 1); // U from 0 to 1
				double v = double(j) / (m_image_height - 1); // V from 1 to 0

				Ray r = m_camera.get_ray(u, v);
				pixel_color += ray_color(r, world, m_max_bounces);
			}

			int64_t idx = (m_image_height - j - 1) * m_image_width + i;
			WritePixelAsColorRef(m_frame_buffer[idx], pixel_color);
		}

		// Visual aid: mark next row before rendering
		for (int i = 0; i < m_image_width && j > 0; i++)
		{
			color pixel_color(1, 0, 0);
			int64_t idx = (m_image_height - j) * m_image_width + i;
			WritePixelAsColorRef(m_frame_buffer[idx], pixel_color);
		}

		PresentToDisplay();
	}

	// Timing
	std::chrono::steady_clock::time_point end_time = high_resolution_clock::now();
	duration<double, std::milli> ms_double = end_time - start_time;
	std::cout << std::endl << "\t(" << ms_double.count() << ") ms\n\n";
}

void Renderer::PresentToDisplay()
{
	// Create a bitmap using the pixels in the bitmap array
	HBITMAP map = CreateBitmap(m_image_width, m_image_height, 1, 8 * 4, (void*)m_frame_buffer);
	CHECK_ERROR(map != nullptr, "Failed to create bitmap.");

	// Create a temp HDC to copy the generated bitmap into
	HDC hdcSrc = CreateCompatibleDC(m_device_context);
	CHECK_ERROR(hdcSrc != nullptr, "Failed to CreateCompatibleDC.");

	// Copy bitmap into HDC
	SelectObject(hdcSrc, map);

	// Copy image from the temp HDC into the destination HDC
	int res = BitBlt(m_device_context, 2500, 10, m_image_width, m_image_height, hdcSrc, 0, 0, SRCCOPY);
	CHECK_ERROR(res != 0, "Failed to BitBlt.");
}

RESULT Renderer::StartRender()
{
	// Render loop
	while (true)
	{
		CastRays();

		// Exit by pressing 'Q' key
		if (GetKeyState('Q') & 0x8000/*Check if high-order bit is set (1 << 15)*/)
		{
			return RESULT::RES_OK;
		}

		m_frame_num++;
	}

	return RESULT::RES_OK;
}

RESULT Renderer::Destroy()
{
	// Cleanup
	ReleaseDC(0, m_device_context);
	//DeleteDC(hdcSrc);

	// TODO, revert desktop to old image from before overwriting
	// better yet, render to a new window instead of directly to desktop

	return RESULT::RES_OK;
}

void Renderer::WritePixelAsColorRef(COLORREF& dest, color c)
{
	double r = c.x();
	double g = c.y();
	double b = c.z();

	double scale = 1.0 / m_sample_size_per_pixel;
	r = sqrt(scale * r);
	g = sqrt(scale * g);
	b = sqrt(scale * b);

	int ir = static_cast<int>(256 * clamp(r, 0.0, 0.999));
	int ig = static_cast<int>(256 * clamp(g, 0.0, 0.999));
	int ib = static_cast<int>(256 * clamp(b, 0.0, 0.999));

	// Clear previous values first
	dest = 0;
	dest |= ir << 16;
	dest |= ig << 8;
	dest |= ib;
}