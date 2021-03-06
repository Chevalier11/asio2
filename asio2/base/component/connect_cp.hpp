/*
 * COPYRIGHT (C) 2017-2021, zhllxt
 *
 * author   : zhllxt
 * email    : 37792738@qq.com
 * 
 * Distributed under the GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
 * (See accompanying file LICENSE or see <http://www.gnu.org/licenses/>)
 */

#ifndef __ASIO2_CONNECT_COMPONENT_HPP__
#define __ASIO2_CONNECT_COMPONENT_HPP__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <memory>
#include <future>
#include <utility>
#include <string_view>

#include <asio2/3rd/asio.hpp>
#include <asio2/base/iopool.hpp>
#include <asio2/base/error.hpp>
#include <asio2/base/listener.hpp>
#include <asio2/base/detail/condition_wrap.hpp>

#include <asio2/base/component/event_queue_cp.hpp>

namespace asio2::detail
{
	template<class derived_t, class args_t, bool IsSession>
	class connect_cp_member_variables;

	template<class derived_t, class args_t>
	class connect_cp_member_variables<derived_t, args_t, true>
	{
	};

	template<class derived_t, class args_t>
	class connect_cp_member_variables<derived_t, args_t, false>
	{
	public:
		using socket_t           = typename args_t::socket_t;
		using raw_socket_t       = typename std::remove_reference_t<socket_t>;
		using resolver_type      = typename asio::ip::basic_resolver<typename raw_socket_t::protocol_type>;
		using endpoints_type     = typename resolver_type::results_type;
		using endpoints_iterator = typename endpoints_type::iterator;

	protected:
		/// Save the host and port of the server
		std::string                     host_, port_;

		/// the endpoints which parsed from host and port
		endpoints_type                  endpoints_;
	};

	/*
	 * can't use "derived_t::is_session()" as the third template parameter of connect_cp_member_variables,
	 * must use "args_t::is_session", beacuse "tcp_session" is derived from "connect_cp", when the 
	 * "connect_cp" is contructed, the "tcp_session" has't contructed yet, then it will can't find the 
	 * "derived_t::is_session()" function, and compile failure.
	 */
	template<class derived_t, class args_t>
	class connect_cp : public connect_cp_member_variables<derived_t, args_t, args_t::is_session>
	{
	public:
		using socket_t           = typename args_t::socket_t;
		using raw_socket_t       = typename std::remove_reference_t<socket_t>;
		using resolver_type      = typename asio::ip::basic_resolver<typename raw_socket_t::protocol_type>;
		using endpoints_type     = typename resolver_type::results_type;
		using endpoints_iterator = typename endpoints_type::iterator;

		using self               = connect_cp<derived_t, args_t>;

	public:
		/**
		 * @constructor
		 */
		connect_cp() {}

		/**
		 * @destructor
		 */
		~connect_cp() = default;

	protected:
		template<bool IsAsync, typename MatchCondition, typename DeferEvent, bool IsSession = args_t::is_session>
		typename std::enable_if_t<!IsSession, void>
		inline _start_connect(std::shared_ptr<derived_t> this_ptr, condition_wrap<MatchCondition> condition,
			DeferEvent&& set_promise)
		{
			derived_t& derive = static_cast<derived_t&>(*this);

			ASIO2_ASSERT(derive.io().strand().running_in_this_thread());

			try
			{
				clear_last_error();

			#if defined(ASIO2_ENABLE_LOG)
				derive.is_stop_reconnect_timer_called_ = false;
				derive.is_stop_connect_timeout_timer_called_ = false;
				derive.is_disconnect_called_ = false;
			#endif

				ASIO2_LOG(spdlog::level::debug, "call connect : {}",
					magic_enum::enum_name(derive.state_.load()));

				state_t expected = state_t::starting;
				if (!derive.state_.compare_exchange_strong(expected, state_t::starting))
				{
					ASIO2_ASSERT(false);
					asio::detail::throw_error(asio::error::operation_aborted);
				}

				auto & socket = derive.socket().lowest_layer();

				error_code ec_ignore{};

				socket.close(ec_ignore);

				socket.open(derive.local_endpoint().protocol());

				// open succeeded. set the keeplive values
				socket.set_option(typename raw_socket_t::reuse_address(true)); // set port reuse

				if constexpr (std::is_same_v<typename raw_socket_t::protocol_type, asio::ip::tcp>)
				{
					derive.keep_alive_options();
				}
				else
				{
					std::ignore = true;
				}

				derive._fire_init();

				socket.bind(derive.local_endpoint());

				// start the timeout timer
				derive._post_connect_timeout_timer(derive.connect_timeout(), this_ptr,
				[&derive, set_promise = std::move(set_promise)]
				(const error_code& ec) mutable
				{
					// no errors indicating that the connection timed out
					if (!ec)
					{
						// we close the socket, so the async_connect will returned 
						// with operation_aborted.
						error_code ec_ignore{};
						derive.socket().lowest_layer().close(ec_ignore);
					}

					error_code connect_ec = derive._connect_error_code();

					set_last_error(connect_ec ? connect_ec :
						(ec == asio::error::operation_aborted ? error_code{} : ec));
				});

				derive._post_resolve(std::move(this_ptr), std::move(condition));
			}
			catch (system_error const& e)
			{
				set_last_error(e);

				derive._handle_connect(e.code(), std::move(this_ptr), std::move(condition));
			}
			catch (std::exception const&)
			{
				error_code ec = asio::error::invalid_argument;

				set_last_error(ec);

				derive._handle_connect(ec, std::move(this_ptr), std::move(condition));
			}
		}

		template<typename MatchCondition, bool IsSession = args_t::is_session>
		typename std::enable_if_t<!IsSession, void>
		inline _post_resolve(std::shared_ptr<derived_t> this_ptr, condition_wrap<MatchCondition> condition)
		{
			derived_t& derive = static_cast<derived_t&>(*this);

			std::string_view host, port;

			if constexpr (condition_helper::has_socks5<MatchCondition>())
			{
				auto& sock5 = condition.impl_->socks5_option(std::in_place);

				host = sock5.host();
				port = sock5.port();
			}
			else
			{
				host = this->host_;
				port = this->port_;
			}

			// resolve the server address.
			std::unique_ptr<resolver_type> resolver_ptr = std::make_unique<resolver_type>(
				derive.io().context());

			resolver_type* resolver_rptr = resolver_ptr.get();

			// Before async_resolve execution is complete, we must hold the resolver object.
			// so we captured the resolver_ptr into the lambda callback function.
			resolver_rptr->async_resolve(host, port, asio::bind_executor(derive.io().strand(),
			[this, &derive, this_ptr = std::move(this_ptr), condition = std::move(condition),
				resolver_ptr = std::move(resolver_ptr)]
			(const error_code& ec, const endpoints_type& endpoints) mutable
			{
				set_last_error(ec);

				this->endpoints_ = endpoints;

				if (ec)
					derive._handle_connect(ec, std::move(this_ptr), std::move(condition));
				else
					derive._post_connect(ec, this->endpoints_.begin(),
						std::move(this_ptr), std::move(condition));
			}));
		}

		template<typename MatchCondition, bool IsSession = args_t::is_session>
		typename std::enable_if_t<!IsSession, void>
		inline _post_connect(error_code ec, endpoints_iterator iter, std::shared_ptr<derived_t> this_ptr,
			condition_wrap<MatchCondition> condition)
		{
			derived_t& derive = static_cast<derived_t&>(*this);

			ASIO2_ASSERT(derive.io().strand().running_in_this_thread());

			try
			{
				if (iter == this->endpoints_.end())
				{
					// There are no more endpoints to try. Shut down the client.
					derive._handle_connect(ec ? ec : asio::error::host_unreachable,
						std::move(this_ptr), std::move(condition));

					return;
				}

				// Start the asynchronous connect operation.
				derive.socket().lowest_layer().async_connect(iter->endpoint(),
					asio::bind_executor(derive.io().strand(), make_allocator(derive.rallocator(),
				[&derive, iter, this_ptr = std::move(this_ptr), condition = std::move(condition)]
				(const error_code & ec) mutable
				{
					set_last_error(ec);

					if (ec && ec != asio::error::operation_aborted)
						derive._post_connect(ec, ++iter,
							std::move(this_ptr), std::move(condition));
					else
						derive._post_proxy(ec, std::move(this_ptr), std::move(condition));
				})));
			}
			catch (system_error & e)
			{
				set_last_error(e);

				derive._handle_connect(e.code(), std::move(this_ptr), std::move(condition));
			}
		}

		template<typename MatchCondition>
		inline void _post_proxy(const error_code& ec, std::shared_ptr<derived_t> this_ptr,
			condition_wrap<MatchCondition> condition)
		{
			set_last_error(ec);

			derived_t& derive = static_cast<derived_t&>(*this);

			if constexpr (is_template_instance_of_v<ecs_t, MatchCondition>)
			{
				if (ec)
					return derive._handle_proxy(ec, std::move(this_ptr), std::move(condition));

				//// Traverse each component in order, and if it is a proxy component, start it
				//detail::for_each_tuple(condition.impl_->components_, [&](auto& component) mutable
				//{
				//	using type = detail::remove_cvref_t<decltype(component)>;

				//	if constexpr (std::is_base_of_v<asio2::socks5::detail::option_base, type>)
				//		derive._socks5_start(this_ptr, condition);
				//	else
				//	{
				//		std::ignore = true;
				//	}
				//});

				if constexpr (MatchCondition::has_socks5())
					derive._socks5_start(std::move(this_ptr), std::move(condition));
				else
					derive._handle_proxy(ec, std::move(this_ptr), std::move(condition));
			}
			else
			{
				derive._handle_proxy(ec, std::move(this_ptr), std::move(condition));
			}
		}

		template<typename MatchCondition>
		inline void _handle_proxy(const error_code& ec, std::shared_ptr<derived_t> this_ptr,
			condition_wrap<MatchCondition> condition)
		{
			set_last_error(ec);

			derived_t& derive = static_cast<derived_t&>(*this);

			derive._handle_connect(ec, std::move(this_ptr), std::move(condition));
		}

		template<typename MatchCondition>
		inline void _handle_connect(const error_code& ec, std::shared_ptr<derived_t> this_ptr,
			condition_wrap<MatchCondition> condition)
		{
			set_last_error(ec);

			derived_t& derive = static_cast<derived_t&>(*this);

			if constexpr (args_t::is_session)
			{
				ASIO2_ASSERT(derive.sessions().io().strand().running_in_this_thread());
			}
			else
			{
				ASIO2_ASSERT(derive.io().strand().running_in_this_thread());
			}

			derive._done_connect(ec, std::move(this_ptr), std::move(condition));
		}

		template<typename MatchCondition>
		inline void _done_connect(error_code ec, std::shared_ptr<derived_t> this_ptr,
			condition_wrap<MatchCondition> condition)
		{
			derived_t& derive = static_cast<derived_t&>(*this);

			ASIO2_LOG(spdlog::level::debug, "enter _done_connect : {}",
				magic_enum::enum_name(derive.state_.load()));

			// code run to here, the state must not be stopped( stopping is possible but stopped is impossible )
			//ASIO2_ASSERT(derive.state() != state_t::stopped);

			try
			{
				// if connect timeout is true, reset the error to timed_out.
				if (derive._is_connect_timeout())
				{
					ec = asio::error::timed_out;
				}

				// Whatever of connection success or failure or timeout, cancel the timeout timer.
				derive._stop_connect_timeout_timer(ec);

				state_t expected;

				// Set the state to started before fire_connect because the user may send data in
				// fire_connect and fail if the state is not set to started.
				if (!ec)
				{
					expected = state_t::starting;
					if (!derive.state().compare_exchange_strong(expected, state_t::started))
						ec = asio::error::operation_aborted;
				}

				set_last_error(ec);

				// Is session : Only call fire_connect notification when the connection is succeed.
				if constexpr (args_t::is_session)
				{
					ASIO2_ASSERT(derive.sessions().io().strand().running_in_this_thread());

					if (!ec)
					{
						expected = state_t::started;
						if (derive.state().compare_exchange_strong(expected, state_t::started))
						{
							derive._fire_connect(this_ptr, condition);
						}
					}
				}
				// Is client : Whether the connection succeeds or fails, always call fire_connect notification
				else
				{
					ASIO2_ASSERT(derive.io().strand().running_in_this_thread());

					// if state is not stopped, call _fire_connect
					expected = state_t::stopped;
					if (!derive.state().compare_exchange_strong(expected, state_t::stopped))
					{
						derive._fire_connect(this_ptr, ec, condition);
					}
				}

				if (!ec)
				{
					expected = state_t::started;
					if (!derive.state().compare_exchange_strong(expected, state_t::started))
						asio::detail::throw_error(asio::error::operation_aborted);
				}

				asio::detail::throw_error(ec);

				derive._do_start(std::move(this_ptr), std::move(condition));
			}
			catch (system_error & e)
			{
				set_last_error(e);

				ASIO2_LOG(spdlog::level::debug, "call _do_disconnect by _done_connect error : {} {} {}",
					magic_enum::enum_name(derive.state_.load()), e.code().value(), e.what());

				derive._do_disconnect(e.code());
			}
		}
	};
}

#endif // !__ASIO2_CONNECT_COMPONENT_HPP__
