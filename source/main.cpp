
#include <limbus/OpenglWindow.hpp>
#include <limbus/Timer.hpp>
#include <limbus/Keyboard.hpp>
#include <limbus/opengl.h>
#include <pingo/SpriteBuffer.hpp>
#include <pingo/Texture.hpp>
#include <glm/glm.hpp>

#include <iostream>
#include <sstream>
#include <complex>
#include <cmath>
#include <limits>
#include <thread>
#include <chrono>
#include <vector>

class Raytracer
{
public:
	struct PointLight
	{
		glm::vec3 position;
		glm::vec3 color;

		PointLight(glm::vec3 position, glm::vec3 color) : position(position), color(color) {}
		PointLight() {}
	};

	struct Ray
	{
		glm::vec3 position;
		glm::vec3 normal;

		Ray(glm::vec3 position, glm::vec3 normal) : position(position), normal(normal) {}
	};

	struct Geometry
	{
		struct Material
		{
			glm::vec3 diffuse_color;

			glm::vec3 specular_color;
			float specular_power;

			float reflection_factor;

			float refraction_factor;
			float refraction_index;

			Material() : specular_power(1.0f), reflection_factor(0.0f), refraction_factor(0.0f) {}
		};

		virtual float intersectRay(const Ray &ray, glm::vec3& intersection, float max_depth = FLT_MAX) const = 0;
		virtual glm::vec3 getNormal(const glm::vec3& intersection) const = 0;
		virtual const Material& getMaterial() const = 0;
	};

	struct Sphere : Geometry
	{
		glm::vec3 position;
		float radius;

		Material material;

		Sphere(glm::vec3 position, float radius, const Material& material) : position(position), radius(radius), material(material) {}

		float intersectRay(const Ray &ray, glm::vec3& intersection, float max_depth) const
		{
			glm::vec3 l = position - ray.position;
			float s = glm::dot(l, ray.normal);
			float l2 = glm::dot(l, l);
			float r2 = radius*radius;
			if (s < 0 && l2 > r2)
				return 0.0f;

			float m2 = l2 - s*s;
			if (m2 > r2)
				return 0.0f;

			float q = glm::sqrt(r2 - m2);
			float t;
			float result = 1.0f;
			if (l2 > r2)
				t = s - q;
			else
			{
				t = s + q;
				result = -1.0f;
			}

			if (t > max_depth)
				return 0.0f;
			
			intersection = ray.position + ray.normal * t;
			return result;
		}

		glm::vec3 getNormal(const glm::vec3& intersection) const
		{
			return glm::normalize(intersection - position);
		}

		const Material& getMaterial() const
		{
			return material;
		}
	};

	struct Plane : Geometry
	{
		glm::vec3 normal;
		float d;

		Material material;

		Plane(glm::vec3 normal, float d, const Material& material) : normal(normal), d(d), material(material) {}

		float intersectRay(const Ray &ray, glm::vec3& intersection, float max_depth) const
		{
			float angle = glm::dot(normal, ray.normal);
			if (angle == 0.0f)
				return 0.0f;

			float result = 1.0f;
			if (angle > 0.0f)
				result = -1.0f;

			float t = -(glm::dot(normal, ray.position) + d) / angle;
			if (t < 0.0f)
				return 0.0f;

			if (t > max_depth)
				return 0.0f;

			intersection = ray.position + ray.normal * t;
			return result;
		}

		glm::vec3 getNormal(const glm::vec3& intersection) const
		{
			return normal;
		}

		const Material& getMaterial() const
		{
			return material;
		}
	};

	struct Job
	{
		size_t y_start;
		size_t y_count;
		size_t max_depth;

		glm::vec3 camera_position;
	};

	static const int max_iterations = 1000;
	static const int max_colors = 50;

	Raytracer(unsigned char* texture_data, double texture_width, double texture_height)
		: texture_data(texture_data), texture_width(texture_width), texture_height(texture_height), job_done(true), running(true)
	{
		aspect_ratio = (float)texture_height / (float)texture_width;
		createScene();
	}

	typedef std::lock_guard<std::mutex> lock_guard;

	void setJob(Job new_job)
	{
		current_job = new_job;
		lock_guard lock(job_mutex);
		job_done = false;
	}

	bool jobDone()
	{
		lock_guard lock(job_mutex);
		return job_done;
	}

	void stop()
	{
		lock_guard lock(running_mutex);
		running = false;
	}

	void operator () ()
	{
		bool _running;

		do
		{
			bool done;
			{
				lock_guard lock(job_mutex);
				done = job_done;
			}

			if (!done)
			{
				for (size_t y = current_job.y_start; y < current_job.y_start + current_job.y_count; ++y)
				{
					for (size_t x = 0; x < (size_t)texture_width; ++x)
					{
						glm::vec3 color(0, 0, 0);
						CalculatePixel(x, y, color);
						WritePixel(x, y, color);
					}
				}

				lock_guard lock(job_mutex);
				job_done = true;
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}

			lock_guard lock(running_mutex);
			_running = running;
		} while (_running);
	}

private:
	Raytracer(Raytracer& other);

	Job current_job;
	bool job_done;
	bool running;

	std::mutex job_mutex, running_mutex;

	double texture_width;
	double texture_height;
	float aspect_ratio;

	unsigned char* texture_data;

	std::vector<Geometry*> geometry_list;
	PointLight point_light;

	void createScene()
	{
		Geometry::Material green;
		green.diffuse_color = glm::vec3(0.1f, 0.1f, 0.1f);
		green.reflection_factor = 0.5f;
		green.specular_power = 100.0f;
		green.specular_color = glm::vec3(1.0f, 1.0f, 1.0f);
		green.refraction_factor = 0.5f;
		green.refraction_index = 1.52f;

		Geometry::Material red;
		red.diffuse_color = glm::vec3(0.9f, 0.0f, 0.0f);
		red.reflection_factor = 0.05f;
		red.specular_power = 3.0f;
		red.specular_color = glm::vec3(1.0f, 1.0f, 1.0f);

		Geometry::Material blue;
		blue.diffuse_color = glm::vec3(0.3, 0.3, 0.9);
		blue.reflection_factor = 0.1f;
		blue.specular_power = 1.0f;
		blue.specular_color = glm::vec3(0.3, 0.3, 0.9);

		geometry_list.push_back(new Sphere(glm::vec3(2.0f, 0.0f, 6.0f), 2.0f, green));
		geometry_list.push_back(new Sphere(glm::vec3(0.0f, 0.0f, 13.0f), 2.0f, red));
		geometry_list.push_back(new Plane(glm::vec3(0.0f, 1.0f, 0.0f), 3.0f, blue));
		
		point_light = PointLight(glm::vec3(0.0f, 10.0f, -5.0f), glm::vec3(0.3f, 0.2f, 0.1f) * 3.0f);
	}

	void CalculatePixel(size_t x, size_t y, glm::vec3& color) const
	{
		float normalized_x = (float)x / (float)texture_width - 0.5f;
		float normalized_y = ((1.0f - (float)y / (float)texture_height) - 0.5f) * aspect_ratio;

		glm::vec3 ray_origin = current_job.camera_position;
		Ray ray(ray_origin, glm::normalize(glm::vec3(normalized_x, normalized_y, 0.5f)));

		raytrace(ray, color);
	}

	void raytrace(const Ray& ray, glm::vec3& color, size_t depth = 0) const
	{
		const float reflection_epsilon = 0.0002f;
		const float pi = 3.141592653589793238462643383279502884197169399375105820974944f;
		const float space_refraction_index = 1.000277f;

		if (depth > current_job.max_depth)
			return;

		glm::vec3 intersection;
		for (std::vector<Geometry*>::const_iterator it = geometry_list.begin(); it != geometry_list.end(); ++it)
		{
			Geometry* geometry = *it;
			float result = geometry->intersectRay(ray, intersection);
			if (result != 0.0f)
			{
				glm::vec3 light_direction = glm::normalize(point_light.position - intersection);
				const Geometry::Material& material = geometry->getMaterial();
				glm::vec3 intersection_normal = geometry->getNormal(intersection);

				if (result > 0.0f)
				{
					// Shadows
					float shadow_factor = 1.0f;
					{
						Ray shadow_ray(intersection + light_direction * reflection_epsilon, light_direction);
						glm::vec3 shadow_intersection;
						for (std::vector<Geometry*>::const_iterator it = geometry_list.begin(); it != geometry_list.end(); ++it)
						{
							Geometry* shadow_geometry = *it;
							if (shadow_geometry->intersectRay(shadow_ray, shadow_intersection, glm::length(point_light.position - intersection)) != 0.0f)
							{
								shadow_factor = 0.5f;
							}
						}
					}

					// Lambertian lighting
					{
						glm::vec3 h = glm::normalize(-ray.normal + light_direction);
						// Diffuse color
						color += (material.diffuse_color / pi) * point_light.color * glm::clamp(glm::dot(light_direction, intersection_normal), 0.0f, 1.0f) * shadow_factor;
						// Specular color
						color += ((material.specular_power + 8) / (8 * pi)) * glm::pow(glm::clamp(glm::dot(h, intersection_normal), 0.0f, 1.0f), material.specular_power) * material.specular_color * (point_light.color * glm::clamp(glm::dot(light_direction, intersection_normal), 0.0f, 1.0f)) * shadow_factor;
					}

					// Reflection
					if (material.reflection_factor > 0.0f)
					{
						glm::vec3 reflection_normal(ray.normal - 2.0f * glm::dot(ray.normal, intersection_normal) * intersection_normal);
						Ray reflection_ray(intersection + reflection_normal * reflection_epsilon, reflection_normal);

						glm::vec3 reflection_color;
						raytrace(reflection_ray, reflection_color, depth + 1);
						color += material.reflection_factor * reflection_color;
					}
				}

				// Refraction
				if (material.refraction_factor > 0.0f)
				{
					float n = space_refraction_index / material.refraction_index;
					glm::vec3 N = intersection_normal * result;
					float cosI = -glm::dot(N, ray.normal);
					float cosT2 = 1.0f - n * n * (1.0f - cosI * cosI);
					if (cosT2 > 0.0f)
					{
						glm::vec3 T = n * ray.normal + (n * cosI - glm::sqrt(cosT2)) * N;
						glm::vec3 refraction_color;
						raytrace(Ray(intersection + T * reflection_epsilon, T), refraction_color, depth + 1);
						color += refraction_color;
					}
				}

				return;
			}
		}
	}

	void WritePixel(size_t x, size_t y, glm::vec3 color)
	{
		size_t offset = y * (size_t)texture_width + x;
		texture_data[offset * 3 + 0] = (unsigned char)(glm::clamp(color.r, 0.0f, 1.0f) * 255.0);
		texture_data[offset * 3 + 1] = (unsigned char)(glm::clamp(color.g, 0.0f, 1.0f) * 255.0);
		texture_data[offset * 3 + 2] = (unsigned char)(glm::clamp(color.b, 0.0f, 1.0f) * 255.0);
	}
};

class Application : public Limbus::OpenglWindow::EventHandler, public Limbus::Keyboard::EventHandler
{
	bool running;

	void onClose(Limbus::OpenglWindow& self)
	{
		running = false;
	}

	bool keys[256];
	void onKeyEvent(Limbus::Keyboard& self, int key, bool pressed)
	{
		keys[key] = pressed;

		if (pressed)
		{
			if (key == LBKeyRight)
			{
				selection += 1;
				if (selection >= 2)
					selection = 0;
			}

			if (key == LBKeyLeft)
			{
				selection -= 1;
				if (selection < 0)
					selection = 1;
			}

			if (key == LBKeyUp)
			{
				if (selection == 0)
				{
					threads += 1;
					createThreads();
				}
				else if (selection == 1)
				{
					max_depth += 1;
					createThreads();
				}
			}

			if (key == LBKeyDown)
			{
				if (selection == 0)
				{
					threads -= 1;
					if (threads == 0)
						threads = 1;
					createThreads();
				}
				else if (selection == 1)
				{
					max_depth -= 1;
					if (max_depth < 0)
						max_depth = 0;
					createThreads();
				}
			}
		}
	}

	Limbus::OpenglWindow window;
	Pingo::SpriteBuffer* sprite_buffer;
	Pingo::Texture* texture;
	unsigned char* texture_data;

public:
	Limbus::Timer delta_timer;

	int selection;
	size_t threads;
	int max_depth;
	
	glm::vec3 camera_position;

	std::vector<Raytracer*> workers;
	std::vector<std::thread*> worker_threads;

	void stopThreads()
	{
		for (std::vector<Raytracer*>::iterator it = workers.begin(); it != workers.end(); ++it)
		{
			(*it)->stop();
		}

		for (std::vector<std::thread*>::iterator it = worker_threads.begin(); it != worker_threads.end(); ++it)
		{
			(*it)->join();
            delete *it;
		}

		worker_threads.clear();

		for (std::vector<Raytracer*>::iterator it = workers.begin(); it != workers.end(); ++it)
			delete (*it);
		workers.clear();
	}

	void createThreads()
	{
		stopThreads();

		for (size_t i = 0; i < threads; ++i)
		{
			workers.push_back(new Raytracer(texture_data, (double)window.getWidth(), (double)window.getHeight()));
			Raytracer* worker = workers[workers.size() - 1];
			worker_threads.push_back(new std::thread([&](){
                (*worker)();
            }));
		}

		assignJobs();
	}

	void assignJobs()
	{
		size_t max_lines = window.getHeight();
		size_t step = max_lines / threads;
		size_t lines_assigned = 0;

		Raytracer::Job job;
		for (size_t i = 0; i < threads; ++i)
		{
			job.y_start = lines_assigned;
			
			if (i < threads - 1)
				job.y_count = step;
			else
				job.y_count = max_lines - lines_assigned;

			job.max_depth = max_depth;

			job.camera_position = camera_position;

			lines_assigned += step;

			Raytracer* worker = workers[i];
			worker->setJob(job);
		}
	}

	void run()
	{
		running = true;
		window.setCaption("Raytracing Demo");
		window.addEventHandler(this);
		window.setWidth(800);
		window.setHeight(600);
		window.create();

		Limbus::Keyboard keyboard;
		keyboard.addEventHandler(this);
        window.addInputDevice(&keyboard);

        Pingo::Context context(&window);

		texture_data = new unsigned char[window.getWidth() * window.getHeight() * 3];

		texture = new Pingo::Texture(&context);
		texture->loadFromMemory(texture_data, window.getWidth(), window.getHeight(), 3);
		sprite_buffer = new Pingo::SpriteBuffer(&context, texture, 1, true);
		
		sprite_buffer->setWritable(true);
		sprite_buffer->setRectangle(0, 0.0f, 0.0f, (float)window.getWidth(), (float)window.getHeight());
		sprite_buffer->setColor(0, 1.0f, 1.0f, 1.0f, 1.0f);
		sprite_buffer->setTextureRectangle(0, 0.0f, 0.0f, (float)window.getWidth(), (float)window.getHeight());
		sprite_buffer->setWritable(false);

		camera_position = glm::vec3(0.0f, 0.0f, -0.5f);

		selection = 0;
		threads = 8;
		max_depth = 7;
		createThreads();

		Limbus::Timer timer, fps_timer, average_timer;
		double average_elapsed = 1.0f;
		size_t average_frames = 1;
		
		memset(keys, 0, 256 * sizeof(bool));

		while (running)
		{
			double elapsed = 1.0f + timer.getElapsed() * 0.005;

			window.pollEvents();
			keyboard.pollEvents();

			const float speed = 2.0f;
			double dt = delta_timer.getElapsed();
			if (keys[LBKeyA])
				camera_position.x -= speed * dt;
			if (keys[LBKeyD])
				camera_position.x += speed * dt;
			if (keys[LBKeyW])
				camera_position.z += speed * dt;
			if (keys[LBKeyS])
				camera_position.z -= speed * dt;
			if (keys[LBKeyE])
				camera_position.y += speed * dt;
			if (keys[LBKeyQ])
				camera_position.y -= speed * dt;
			delta_timer.reset();

			bool done = true;
			for (std::vector<Raytracer*>::iterator it = workers.begin(); it != workers.end(); ++it)
			{
				if (!(*it)->jobDone())
				{
					done = false;
					break;
				}
			}
			if (done)
			{
                std::cout << "Doing it" << std::endl;
				texture->update(texture_data);
				double elapsed = fps_timer.getElapsed();
				fps_timer.reset();
				std::stringstream stream;
				stream << "Raytracing Demo | ";
				if (selection == 0)
					stream << "Threads: " << threads;
				else if (selection == 1)
					stream << "Max depth: " << max_depth;
				stream << " | Average FPS: " << 1.0f / average_elapsed << " (" << average_elapsed * 1000.0f << " ms) | FPS: " << 1.0f / elapsed << " (" << elapsed * 1000.0f << " ms)";
				window.setCaption(stream.str().c_str());

				average_frames += 1;
				if (average_frames == 10)
				{
					average_elapsed = average_timer.getElapsed() / average_frames;
					average_timer.reset();
					average_frames = 0;
				}

				assignJobs();
			}

			sprite_buffer->draw(0, 1, 0, 0);
			window.swapBuffers();
		}

		stopThreads();

		delete sprite_buffer;
		delete texture;
		delete [] texture_data;
	}
};

int main()
{
	Application app;
	app.run();
    
    return EXIT_SUCCESS;
}
