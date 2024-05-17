#pragma once

#include <DirectXMath.h>

namespace twen 
{
	template<typename T>
	class ConstantBuffer;
	inline namespace Constants 
	{
		inline static constexpr float pi = 3.1415926535f;
	}
	class Camera 
	{
	public:
		enum DataType { ViewProjection, Position, DirectionFront, DirectionUp, DirectionRight };

		using Matrix =::DirectX::XMMATRIX;
		using Vector =::DirectX::XMVECTOR;
	private:
		template<Camera::DataType>
		struct RequiresData;
		template<Camera::DataType...types>
		struct DECLSPEC_EMPTY_BASES RetrieveHelperT : RequiresData<types>...
		{
			template<Camera::DataType type>
			auto const& At() const { return RequiresData<type>::_; }
		};
	public:
		constexpr Camera() = default;
	public:
		template<Camera::DataType...types>
		inline RetrieveHelperT<types...> Retrieve() const noexcept
		{ return { this->*(RequiresData<types>::value)... }; }

		inline void FOV(float value) { assert(value >= 0.00001f);
									   m_FOV = value; }
		inline void Near(float value) { assert(value >= 0.00001f);
										m_Near = value; }
		inline void Far(float value) { assert(value >= 0.00001f);
									   m_Far = value; }
		inline void AspectRadio(float value) { assert(value >= 0.0001f);
											   m_AspectRatio = value;}
		inline void RightHand(bool value) {m_IsRightHand = value;}

		inline void Yaw(float angle);
		inline void Rotate(float angle);

		inline void Move(float x, float y, float z);

		inline void MoveFront(float distance);
		inline void MoveRight(float distance);
		inline void MoveUp(float distance);

		void Update();
	private:
		float m_AspectRatio { 1.33333f }; // Default is 800 / 600;
		float m_FOV			{ Constants::pi / 4.0f };
		float m_Near		{0.1f};
		float m_Far			{100.0f};

		Vector m_Position			{4.0f, 1.0f, 5.0f};
		Vector m_DirectionFront		{ ::DirectX::XMVector3Normalize(::DirectX::XMVectorNegate(m_Position)) };
		Vector m_DirectionUp		{ ::DirectX::XMVector3Normalize(::DirectX::XMVector3Cross(m_Position, ::DirectX::XMVectorAdd(m_Position, {0.1f}))) };
		Vector m_DirectionRight		{ ::DirectX::XMVector3Normalize(::DirectX::XMVector3Cross(m_DirectionFront, m_DirectionUp)) };

		bool m_IsRightHand  {false};

		Matrix m_ViewProjection
		{
		m_IsRightHand ?
			::DirectX::XMMatrixMultiply(
				::DirectX::XMMatrixLookToRH(m_Position, ::DirectX::XMVectorScale(m_DirectionFront, 5.0f), m_DirectionUp),
				::DirectX::XMMatrixPerspectiveFovRH(m_FOV, m_AspectRatio, m_Near, m_Far))
			: ::DirectX::XMMatrixMultiply(
				::DirectX::XMMatrixLookToLH(m_Position, ::DirectX::XMVectorScale(m_DirectionFront, 5.0f), m_DirectionUp),
				::DirectX::XMMatrixPerspectiveFovLH(m_FOV, m_AspectRatio, m_Near, m_Far)
		)};
	};
#define MAKE_ROTATE_QUATERNION(axis)\
{ sin * axis.m128_f32[0], sin * axis.m128_f32[1], sin * axis.m128_f32[2], cos } 
	// Fuck you microsoft. This world would only you can hide real part in vector tail.
	inline void Camera::Yaw(float angle)
	{
		using namespace::DirectX;
		//float sin, cos;
		//XMScalarSinCos(&sin, &cos, angle * 0.5f);
		XMVECTOR quaternion // MAKE_ROTATE_QUATERNION(m_DirectionUp);
							= XMQuaternionRotationAxis(m_DirectionRight, angle);
		m_DirectionUp = XMVector3Rotate(m_DirectionUp, quaternion);
		m_DirectionFront = XMVector3Rotate(m_DirectionFront, quaternion);
	}
	inline void Camera::Rotate(float angle)
	{
		using namespace::DirectX;

		float sin, cos;
		XMScalarSinCos(&sin, &cos, angle * 0.5f);
		XMVECTOR w MAKE_ROTATE_QUATERNION(m_DirectionUp);
		XMVECTOR quaternion // MAKE_ROTATE_QUATERNION(m_DirectionUp);
							= XMQuaternionRotationAxis(m_DirectionUp, angle);
		m_DirectionRight = XMVector3Rotate(m_DirectionRight, quaternion);
		m_DirectionFront = XMVector3Rotate(m_DirectionFront, quaternion);

		auto len = XMVector3Length(m_DirectionFront);
		assert(len.m128_f32[0] >= 0.98f && len.m128_f32[0] <= 1.02f);
		len = XMVector3Length(m_DirectionRight);
		assert(len.m128_f32[0] >= 0.98f && len.m128_f32[0] <= 1.02f);
	}
	inline void Camera::Move(float x, float y, float z)
	{
		using namespace::DirectX;
		m_Position += {x, y, z, 0.0f};
	}
	inline void Camera::MoveFront(float distance)
	{
		using namespace::DirectX;

		m_Position = XMVectorAdd(m_Position, XMVectorScale(m_DirectionFront, distance));
	}
	inline void Camera::MoveRight(float distance)
	{
		using namespace::DirectX;

		m_Position = XMVectorAdd(m_Position, XMVectorScale(m_DirectionRight, distance));
	}
	inline void Camera::MoveUp(float distance)
	{
		using namespace::DirectX;

		m_Position = XMVectorAdd(m_Position, XMVectorScale(m_DirectionUp, distance));
	}
	inline void Camera::Update()
	{
		using namespace::DirectX;

		m_ViewProjection = m_IsRightHand ?
			XMMatrixMultiply(XMMatrixLookToLH(m_Position, m_DirectionFront, m_DirectionUp), XMMatrixPerspectiveFovLH(m_FOV, m_AspectRatio, m_Near, m_Far))
		  : XMMatrixMultiply(XMMatrixLookToRH(m_Position, m_DirectionFront, m_DirectionUp), XMMatrixPerspectiveFovRH(m_FOV, m_AspectRatio, m_Near, m_Far));
	}

#define TABLE_HELPER(type, x) \
	template<>\
	struct Camera::RequiresData<Camera::x>\
	{\
		static constexpr auto value = &Camera::m_##x;\
		type _;\
	}\

	TABLE_HELPER(Camera::Matrix, ViewProjection);
	TABLE_HELPER(Camera::Vector, Position);
	TABLE_HELPER(Camera::Vector, DirectionFront);
	TABLE_HELPER(Camera::Vector, DirectionUp);
	TABLE_HELPER(Camera::Vector, DirectionRight);

#undef TABLE_HELPER
#undef MAKE_ROTATE_QUATERNION
}