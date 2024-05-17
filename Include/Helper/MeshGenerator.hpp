#pragma once

namespace twen::Meshes
{
	struct Vec3
	{
		float x, y, z;
		
		template<typename T>
		operator T();
	};
	struct Vec2 
	{
		float x, y;

		template<typename T>
		operator T();
	};

	struct Column 
	{
		float End;
		float Division;
	};

	struct Generator 
	{
		// Counter clock generate.
		// @param r : radius
		// @param h : height
		static auto Generate(const float r, const float h, unsigned int division = 4u)
		{
			static constexpr auto doublePi{ 3.1415926535f * 2.f };
			//static constexpr auto pi{ 3.1415926535f };

			const float stride{ doublePi / division };
			const float half{h * 0.5f};

			struct 
			{
				::std::vector<Vec3> Pos{};
				::std::vector<Vec2> UV{};
			} result;

			// because enclosed and no indices.
			for (auto i{ 0u }; i < division; i++) 
			{
				GenerateFacePosition(result.Pos, i, stride, half, r);
				GenerateFaceUV(result.UV, i, division);
			}

			return result;
		}

		inline static void GenerateFacePosition(::std::vector<Vec3>& result, ::std::uint32_t index, float stride, float half, float r)
		{
			auto cos0{ ::std::cos(index * stride) }, sin0{ ::std::sin(index * stride) };
			auto cos1{ ::std::cos((index + 1) * stride) }, sin1{::std::sin((index + 1) * stride)};
			auto x0{ r * cos0 }, z0{ r * sin0 };
			auto x1{ r * cos1 }, z1{ r * sin1 };

			// side
			result.emplace_back(x0, -half, z0);
			result.emplace_back(x0, half, z0);
			result.emplace_back(x1, -half, z1);
			
			result.emplace_back(x1, half, z1);
			result.emplace_back(x1, -half, z1);
			result.emplace_back(x0, half, z0);

			// top
			result.emplace_back(0.0f, half, 0.0f);
			result.emplace_back(x1, half, z1);
			result.emplace_back(x0, half, z0);
			// bottom
			result.emplace_back(0.0f, -half, 0.0f);
			result.emplace_back(x1, -half, z1);
			result.emplace_back(x0, -half, z0);
		}
		inline static void GenerateFaceUV(::std::vector<Vec2>& result, ::std::uint32_t index, ::std::uint32_t division)
		{
			// side.
			result.emplace_back(1.0f, 1.0f);
			result.emplace_back(1.0f, 0.0f);
			result.emplace_back(0.0f, 1.0f);

			result.emplace_back(0.0f, 0.0f);
			result.emplace_back(0.0f, 1.0f);
			result.emplace_back(1.0f, 0.0f);

			const float sita = 6.283185307f / division;
			const float u0 = 0.5f + ::std::sin(sita * index) * 0.5f, v0 = 0.5f - ::std::cos(sita * index) * 0.5f;
			const float u1 = 0.5f + ::std::sin(sita * (index + 1)) * 0.5f, v1 = 0.5f - ::std::cos(sita * (index + 1)) * 0.5f;

			// top
			result.emplace_back(0.5f, 0.5f);
			result.emplace_back(u1, v1);
			result.emplace_back(u0, v0);
			// bottom
			result.emplace_back(0.5f, 0.5f);
			result.emplace_back(u1, v1);
			result.emplace_back(u0, v0);
		}
	};
}