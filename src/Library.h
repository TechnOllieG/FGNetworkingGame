#pragma once

struct Vector2
{
	float x = 0;
	float y = 0;

	Vector2(float inX, float inY)
	{
		x = inX;
		y = inY;
	}

	Vector2()
	{
		x = 0;
		y = 0;
	}

	Vector2 operator*(float rhs)
	{
		return Vector2(x * rhs, y * rhs);
	}
	
	Vector2 operator/(float rhs)
	{
		return Vector2(x / rhs, y / rhs);
	}

	void operator+=(Vector2 rhs)
	{
		x += rhs.x;
		y += rhs.y;
	}

	void operator-=(Vector2 rhs)
	{
		x -= rhs.x;
		y -= rhs.y;
	}

	Vector2 operator+(Vector2 rhs)
	{
		return Vector2(x + rhs.x, y + rhs.y);
	}

	Vector2 operator-(Vector2 rhs)
	{
		return Vector2(x - rhs.x, y - rhs.y);
	}

	Vector2 operator-()
	{
		return Vector2(-x, -y);
	}

	float sqrMagnitude()
	{
		return x * x + y * y;
	}

	float magnitude()
	{
		return sqrt(sqrMagnitude());
	}

	Vector2 normalized()
	{
		float length = magnitude();
		Vector2 vector = *this;
		if (length > 0)
		{
			vector.x /= length;
			vector.y /= length;
		}
		return vector;
	}

	void Normalize()
	{
		*this = this->normalized();
	}

	static float Dot(Vector2 lhs, Vector2 rhs)
	{
		return lhs.x * rhs.x + lhs.y * rhs.y;
	}

	static Vector2 Reflect(Vector2 direction, Vector2 normal)
	{
		float projectedVectorOnNormal = Vector2::Dot(direction, normal);
		direction -= (normal * projectedVectorOnNormal).normalized();
		return direction;
	}
};

float clamp(float value, float min, float max)
{
	if (min > max)
	{
		float temp = min;
		min = max;
		max = temp;
	}

	if (value > max)
		return max;

	if (value < min)
		return min;

	return value;
}