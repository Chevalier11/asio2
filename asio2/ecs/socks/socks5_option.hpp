/*
 * COPYRIGHT (C) 2017-2021, zhllxt
 *
 * author   : zhllxt
 * email    : 37792738@qq.com
 * 
 * Distributed under the GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
 * (See accompanying file LICENSE or see <http://www.gnu.org/licenses/>)
 */

#ifndef __ASIO2_SOCKS5_OPTION_HPP__
#define __ASIO2_SOCKS5_OPTION_HPP__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <array>

#include <asio2/ecs/socks/socks5_core.hpp>

namespace asio2::socks5::detail
{
	template<class derived_t, method m>
	class method_field
	{
	};

	template<class derived_t>
	class method_field<derived_t, method::password>
	{
	public:
		method_field() = default;
		method_field(method_field&&) = default;
		method_field(method_field const&) = default;
		method_field& operator=(method_field&&) = default;
		method_field& operator=(method_field const&) = default;

		method_field(std::string username, std::string password)
			: username_(std::move(username))
			, password_(std::move(password))
		{
			ASIO2_ASSERT(username_.size() <= std::size_t(0xff) && password_.size() <= std::size_t(0xff));
		}

		inline derived_t& username(std::string v)
		{
			username_ = std::move(v);
			ASIO2_ASSERT(username_.size() <= std::size_t(0xff));
			return static_cast<derived_t&>(*this);
		}
		inline derived_t& password(std::string v)
		{
			password_ = std::move(v);
			ASIO2_ASSERT(password_.size() <= std::size_t(0xff));
			return static_cast<derived_t&>(*this);
		}

		inline std::string& username() { return  username_; }
		inline std::string& password() { return  password_; }

	protected:
		std::string username_{};
		std::string password_{};
	};

	struct option_base {};

	template<class T, class = void>
	struct has_member_username : std::false_type {};

	template<class T>
	struct has_member_username<T, std::void_t<decltype(std::declval<std::decay_t<T>>().username())>> : std::true_type {};

	template<class T, class = void>
	struct has_member_password : std::false_type {};

	template<class T>
	struct has_member_password<T, std::void_t<decltype(std::declval<std::decay_t<T>>().password())>> : std::true_type {};
}

namespace asio2::socks5
{
	template<method... ms>
	class option : public detail::option_base, public detail::method_field<option<ms...>, ms>...
	{
	public:
		option() = default;
		option(option&&) = default;
		option(option const&) = default;
		option& operator=(option&&) = default;
		option& operator=(option const&) = default;

		// constructor sfinae
		template<class... Args>
		explicit option(Args&&... args) : option(
			std::conditional_t<option<ms...>::has_password_method(),
			std::integral_constant<int, asio2::detail::to_underlying(method::password)>,
			std::integral_constant<int, asio2::detail::to_underlying(method::anonymous)>>{}
		, std::forward<Args>(args)...)
		{
		}

		template<class String1, class String2>
		explicit option(
			std::integral_constant<int, asio2::detail::to_underlying(method::anonymous)>,
			String1&& proxy_host, String2&& proxy_port,
			socks5::command cmd = socks5::command::connect)
			: host_(asio2::detail::to_string(std::forward<String1>(proxy_host)))
			, port_(asio2::detail::to_string(std::forward<String2>(proxy_port)))
			, cmd_ (cmd)
		{
		}

		template<class String1, class String2, class String3, class String4>
		explicit option(
			std::integral_constant<int, asio2::detail::to_underlying(method::password)>,
			String1&& proxy_host, String2&& proxy_port, String3&& username, String4&& password,
			socks5::command cmd = socks5::command::connect)
			: host_(asio2::detail::to_string(std::forward<String1>(proxy_host)))
			, port_(asio2::detail::to_string(std::forward<String2>(proxy_port)))
			, cmd_ (cmd)
		{
			this->username(asio2::detail::to_string(std::forward<String3>(username)));
			this->password(asio2::detail::to_string(std::forward<String4>(password)));
		}

		inline option& host(std::string proxy_host)
		{
			host_ = std::move(proxy_host);
			return (*this);
		}
		inline option& port(std::string proxy_port)
		{
			port_ = std::move(proxy_port);
			return (*this);
		}

		inline std::string& host() { return host_; }
		inline std::string& port() { return port_; }

		// vs2017 15.9.31 not supported
		//constexpr static bool has_password_method = ((ms == method::password) || ...);
		template<typename = void>
		constexpr static bool has_password_method()
		{
			return ((ms == method::password) || ...);
		}

		inline std::array<method, sizeof...(ms)> methods()
		{
			return methods_;
		}

		constexpr auto methods_count() const noexcept
		{
			return methods_.size();
		}

		inline socks5::command command()                    { return cmd_;                }

		inline option&         command(socks5::command cmd) { cmd_ = cmd; return (*this); }

	protected:
		std::array<method, sizeof...(ms)> methods_{ ms... };

		std::string host_{};
		std::string port_{};

		socks5::command cmd_{ socks5::command::connect };
	};
}

#endif // !__ASIO2_SOCKS5_OPTION_HPP__
