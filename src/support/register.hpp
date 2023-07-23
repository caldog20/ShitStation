#pragma once
#include <cassert>
#include <cstdint>

template <typename T, size_t size>
class RegisterSet {
  public:
	RegisterSet() {}

	T& operator[](size_t index) {
		assert(index < size);
		return regs[index];
	}

	T operator[](size_t index) const {
		assert(index < size);
		return regs[index];
	}

	void set(size_t index, T value) {
		assert(index < size);
		regs[index] = value;
	}

	T get(size_t index) {
		assert(index < size);
		return regs[index];
	}

	void reset() {
		for (auto& reg : regs) {
			reg = 0;
		}
	}

  private:
	T regs[size] = {0};
};

//	template <typename T>
//	class Register {
//	  public:
//		Register() {}
//		virtual ~Register() {}
//
//		Register(const Register&) = default;
//		Register(Register&&) = default;
//		Register& operator=(const Register&) = default;
//		Register& operator=(Register&&) = default;
//
//		virtual T get() { return value; }
//		virtual void set(T value) { this->value = value; }
//
//		operator T() {
//			return get();
//		}
//
//		void operator=(T value) {
//			set(value);
//		}
//
//	  private:
//		T value;
//
//	  public:
//		bool operator==(const Register& rhs) const { return value == rhs.value; }
//		bool operator!=(const Register& rhs) const { return rhs != *this; }
//		bool operator<(const Register& rhs) const { return value < rhs.value; }
//		bool operator>(const Register& rhs) const { return rhs < *this; }
//		bool operator<=(const Register& rhs) const { return rhs >= *this; }
//		bool operator>=(const Register& rhs) const { return *this >= rhs; }
//	};

// Generate abstract class for hardware register
#define REGISTER(name, type)                    \
	class name : public Register<type> {        \
	  public:                                   \
		name() {}                               \
		virtual ~name() {}                      \
		name(const name&) = default;            \
		name(name&&) = default;                 \
		name& operator=(const name&) = default; \
		name& operator=(name&&) = default;      \
	};
