/*
 * COPYRIGHT (C) 2017-2021, zhllxt
 *
 * author   : zhllxt
 * email    : 37792738@qq.com
 * 
 * Distributed under the GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
 * (See accompanying file LICENSE or see <http://www.gnu.org/licenses/>)
 * 
 * https://www.zhihu.com/question/25016042
 * Windows API : send 
 * The successful completion of a send function does not indicate that the data 
 * was successfully delivered and received to the recipient. This function only 
 * indicates the data was successfully sent.
 * 
 */

#ifndef __ASIO2_SEND_COMPONENT_HPP__
#define __ASIO2_SEND_COMPONENT_HPP__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <cstdint>
#include <memory>
#include <functional>
#include <string>
#include <future>
#include <queue>
#include <tuple>
#include <utility>
#include <string_view>

#include <asio2/3rd/asio.hpp>
#include <asio2/base/iopool.hpp>
#include <asio2/base/error.hpp>
#include <asio2/base/define.hpp>

#include <asio2/base/detail/util.hpp>
#include <asio2/base/detail/function_traits.hpp>
#include <asio2/base/detail/buffer_wrap.hpp>

#include <asio2/base/component/data_persistence_cp.hpp>

namespace asio2::detail
{
	ASIO2_CLASS_FORWARD_DECLARE_BASE;

	template<class derived_t, class args_t = void>
	class send_cp : public data_persistence_cp<derived_t, args_t>
	{
		ASIO2_CLASS_FRIEND_DECLARE_BASE;

	public:
		/**
		 * @constructor
		 */
		send_cp() {}

		/**
		 * @destructor
		 */
		~send_cp() = default;

	public:
		/**
		 * @function : Asynchronous send data, support multiple data formats,
		 *             see asio::buffer(...) in /asio/buffer.hpp
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * use like this : std::string m; async_send(std::move(m)); can reducing memory allocation.
		 * PodType * : async_send("abc");
		 * PodType (&data)[N] : double m[10]; async_send(m);
		 * std::array<PodType, N> : std::array<int,10> m; async_send(m);
		 * std::vector<PodType, Allocator> : std::vector<float> m; async_send(m);
		 * std::basic_string<Elem, Traits, Allocator> : std::string m; async_send(m);
		 */
		template<class DataT>
		inline void async_send(DataT&& data)
		{
			derived_t& derive = static_cast<derived_t&>(*this);

			// We must ensure that there is only one operation to send data
			// at the same time,otherwise may be cause crash.
			try
			{
				if (!derive.is_started())
					asio::detail::throw_error(asio::error::not_connected);

				derive.push_event(
				[&derive, p = derive.selfptr(), data = derive._data_persistence(std::forward<DataT>(data))]
				(event_queue_guard<derived_t>&& g) mutable
				{
					derive._do_send(data, [g = std::move(g)](const error_code&, std::size_t) mutable {});
				});
			}
			catch (system_error & e) { set_last_error(e); }
			catch (std::exception &) { set_last_error(asio::error::eof); }
		}

		/**
		 * @function : Asynchronous send data
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * PodType * : async_send("abc");
		 */
		template<class CharT, class Traits = std::char_traits<CharT>>
		inline typename std::enable_if_t<
			std::is_same_v<detail::remove_cvref_t<CharT>, char> ||
			std::is_same_v<detail::remove_cvref_t<CharT>, wchar_t> ||
			std::is_same_v<detail::remove_cvref_t<CharT>, char16_t> ||
			std::is_same_v<detail::remove_cvref_t<CharT>, char32_t>, void> async_send(CharT * s)
		{
			derived_t& derive = static_cast<derived_t&>(*this);

			derive.async_send(s, s ? Traits::length(s) : 0);
		}

		/**
		 * @function : Asynchronous send data
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * PodType (&data)[N] : double m[10]; async_send(m,5);
		 */
		template<class CharT, class SizeT>
		inline typename std::enable_if_t<std::is_integral_v<detail::remove_cvref_t<SizeT>>, void>
			async_send(CharT* s, SizeT count)
		{
			derived_t& derive = static_cast<derived_t&>(*this);

			// We must ensure that there is only one operation to send data
			// at the same time,otherwise may be cause crash.
			try
			{
				if (!derive.is_started())
					asio::detail::throw_error(asio::error::not_connected);

				if (!s)
					asio::detail::throw_error(asio::error::invalid_argument);

				derive.push_event([&derive, p = derive.selfptr(), data = derive._data_persistence(s, count)]
				(event_queue_guard<derived_t>&& g) mutable
				{
					derive._do_send(data, [g = std::move(g)](const error_code&, std::size_t) mutable {});
				});
			}
			catch (system_error & e) { set_last_error(e); }
			catch (std::exception &) { set_last_error(asio::error::eof); }
		}

		/**
		 * @function : Asynchronous send data, support multiple data formats,
		 *             see asio::buffer(...) in /asio/buffer.hpp
		 * use like this : std::string m; async_send(std::move(m)); can reducing memory allocation.
		 * the pair.first save the send result error_code,the pair.second save the sent_bytes.
		 * note : Do not call this function in any listener callback function like this:
		 * auto future = async_send(msg,asio::use_future); future.get(); it will cause deadlock and
		 * the future.get() will never return.
		 * PodType * : async_send("abc");
		 * PodType (&data)[N] : double m[10]; async_send(m);
		 * std::array<PodType, N> : std::array<int,10> m; async_send(m);
		 * std::vector<PodType, Allocator> : std::vector<float> m; async_send(m);
		 * std::basic_string<Elem, Traits, Allocator> : std::string m; async_send(m);
		 */
		template<class DataT>
		inline std::future<std::pair<error_code, std::size_t>> async_send(DataT&& data, asio::use_future_t<> flag)
		{
			derived_t& derive = static_cast<derived_t&>(*this);

			// why use copyable_wrapper? beacuse std::promise is moveable-only, but
			// std::function is copyable-only
			std::ignore = flag;
			copyable_wrapper<std::promise<std::pair<error_code, std::size_t>>> promise;
			std::future<std::pair<error_code, std::size_t>> future = promise().get_future();
			try
			{
				if (!derive.is_started())
					asio::detail::throw_error(asio::error::not_connected);

				derive.push_event([&derive, p = derive.selfptr(), promise = std::move(promise),
					data = derive._data_persistence(std::forward<DataT>(data))]
				(event_queue_guard<derived_t>&& g) mutable
				{
					derive._do_send(data, [&promise, g = std::move(g)]
					(const error_code& ec, std::size_t bytes_sent) mutable
					{
						promise().set_value(std::pair<error_code, std::size_t>(ec, bytes_sent));
					});
				});
			}
			catch (system_error & e)
			{
				set_last_error(e);
				promise().set_value(std::pair<error_code, std::size_t>(e.code(), 0));
			}
			catch (std::exception &)
			{
				set_last_error(asio::error::eof);
				promise().set_value(std::pair<error_code, std::size_t>(asio::error::eof, 0));
			}
			return future;
		}

		/**
		 * @function : Asynchronous send data
		 * the pair.first save the send result error_code,the pair.second save the sent_bytes.
		 * note : Do not call this function in any listener callback function like this:
		 * auto future = async_send(msg,asio::use_future); future.get(); it will cause deadlock and
		 * the future.get() will never return.
		 * PodType * : async_send("abc");
		 */
		template<class CharT, class Traits = std::char_traits<CharT>>
		inline typename std::enable_if_t<
			std::is_same_v<detail::remove_cvref_t<CharT>, char> ||
			std::is_same_v<detail::remove_cvref_t<CharT>, wchar_t> ||
			std::is_same_v<detail::remove_cvref_t<CharT>, char16_t> ||
			std::is_same_v<detail::remove_cvref_t<CharT>, char32_t>,
			std::future<std::pair<error_code, std::size_t>>>
			async_send(CharT * s, asio::use_future_t<> flag)
		{
			derived_t& derive = static_cast<derived_t&>(*this);

			return derive.async_send(s, s ? Traits::length(s) : 0, std::move(flag));
		}

		/**
		 * @function : Asynchronous send data
		 * the pair.first save the send result error_code,the pair.second save the sent_bytes.
		 * note : Do not call this function in any listener callback function like this:
		 * auto future = async_send(msg,asio::use_future); future.get(); it will cause deadlock,
		 * the future.get() will never return.
		 * PodType (&data)[N] : double m[10]; async_send(m,5);
		 */
		template<class CharT, class SizeT>
		inline typename std::enable_if_t<std::is_integral_v<detail::remove_cvref_t<SizeT>>,
			std::future<std::pair<error_code, std::size_t>>>
			async_send(CharT * s, SizeT count, asio::use_future_t<> flag)
		{
			derived_t& derive = static_cast<derived_t&>(*this);

			std::ignore = flag;
			copyable_wrapper<std::promise<std::pair<error_code, std::size_t>>> promise;
			std::future<std::pair<error_code, std::size_t>> future = promise().get_future();
			try
			{
				if (!derive.is_started())
					asio::detail::throw_error(asio::error::not_connected);

				if (!s)
					asio::detail::throw_error(asio::error::invalid_argument);

				derive.push_event([&derive, p = derive.selfptr(), data = derive._data_persistence(s, count),
					promise = std::move(promise)](event_queue_guard<derived_t>&& g) mutable
				{
					derive._do_send(data, [&promise, g = std::move(g)]
					(const error_code& ec, std::size_t bytes_sent) mutable
					{
						promise().set_value(std::pair<error_code, std::size_t>(ec, bytes_sent));
					});
				});
			}
			catch (system_error & e)
			{
				set_last_error(e);
				promise().set_value(std::pair<error_code, std::size_t>(e.code(), 0));
			}
			catch (std::exception &)
			{
				set_last_error(asio::error::eof);
				promise().set_value(std::pair<error_code, std::size_t>(asio::error::eof, 0));
			}
			return future;
		}

		/**
		 * @function : Asynchronous send data, support multiple data formats,
		 *             see asio::buffer(...) in /asio/buffer.hpp
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * use like this : std::string m; async_send(std::move(m)); can reducing memory allocation.
		 * PodType * : async_send("abc");
		 * PodType (&data)[N] : double m[10]; async_send(m);
		 * std::array<PodType, N> : std::array<int,10> m; async_send(m);
		 * std::vector<PodType, Allocator> : std::vector<float> m; async_send(m);
		 * std::basic_string<Elem, Traits, Allocator> : std::string m; async_send(m);
		 * Callback signature : void() or void(std::size_t bytes_sent)
		 */
		template<class DataT, class Callback>
		inline typename std::enable_if_t<is_callable_v<Callback>, void> async_send(DataT&& data, Callback&& fn)
		{
			derived_t& derive = static_cast<derived_t&>(*this);

			// We must ensure that there is only one operation to send data
			// at the same time,otherwise may be cause crash.
			try
			{
				if (!derive.is_started())
					asio::detail::throw_error(asio::error::not_connected);

				derive.push_event([&derive, p = derive.selfptr(), data = derive._data_persistence(std::forward<DataT>(data)),
					fn = std::forward<Callback>(fn)](event_queue_guard<derived_t>&& g) mutable
				{
					derive._do_send(data, [&fn, g = std::move(g)]
					(const error_code&, std::size_t bytes_sent) mutable
					{
						ASIO2_ASSERT(g.valid());
						callback_helper::call(fn, bytes_sent);
					});
				});
				return;
			}
			catch (system_error & e) { set_last_error(e); }
			catch (std::exception &) { set_last_error(asio::error::eof); }

			// we should ensure that the callback must be called in the io_context thread.
			derive.dispatch([ec = get_last_error(), fn = std::forward<Callback>(fn)]() mutable
			{
				set_last_error(ec);

				callback_helper::call(fn, 0);
			});
		}

		/**
		 * @function : Asynchronous send data
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * PodType * : async_send("abc");
		 * Callback signature : void() or void(std::size_t bytes_sent)
		 */
		template<class Callback, class CharT, class Traits = std::char_traits<CharT>>
		inline typename std::enable_if_t<is_callable_v<Callback> && (
			std::is_same_v<detail::remove_cvref_t<CharT>, char> ||
			std::is_same_v<detail::remove_cvref_t<CharT>, wchar_t> ||
			std::is_same_v<detail::remove_cvref_t<CharT>, char16_t> ||
			std::is_same_v<detail::remove_cvref_t<CharT>, char32_t>), void>
			async_send(CharT * s, Callback&& fn)
		{
			derived_t& derive = static_cast<derived_t&>(*this);

			derive.async_send(s, s ? Traits::length(s) : 0, std::forward<Callback>(fn));
		}

		/**
		 * @function : Asynchronous send data
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * PodType (&data)[N] : double m[10]; async_send(m,5);
		 * Callback signature : void() or void(std::size_t bytes_sent)
		 */
		template<class Callback, class CharT, class SizeT>
		inline typename std::enable_if_t<is_callable_v<Callback> &&
			std::is_integral_v<detail::remove_cvref_t<SizeT>>, void>
			async_send(CharT * s, SizeT count, Callback&& fn)
		{
			derived_t& derive = static_cast<derived_t&>(*this);

			// We must ensure that there is only one operation to send data
			// at the same time,otherwise may be cause crash.
			try
			{
				if (!derive.is_started())
					asio::detail::throw_error(asio::error::not_connected);

				if (!s)
					asio::detail::throw_error(asio::error::invalid_argument);

				derive.push_event([&derive, p = derive.selfptr(), data = derive._data_persistence(s, count),
					fn = std::forward<Callback>(fn)](event_queue_guard<derived_t>&& g) mutable
				{
					derive._do_send(data, [&fn, g = std::move(g)]
					(const error_code&, std::size_t bytes_sent) mutable
					{
						callback_helper::call(fn, bytes_sent);
					});
				});
				return;
			}
			catch (system_error & e) { set_last_error(e); }
			catch (std::exception &) { set_last_error(asio::error::eof); }

			// we should ensure that the callback must be called in the io_context thread.
			derive.dispatch([ec = get_last_error(), fn = std::forward<Callback>(fn)]() mutable
			{
				set_last_error(ec);

				callback_helper::call(fn, 0);
			});
		}

	public:
		/**
		 * @function : Synchronous send data, support multiple data formats,
		 *             see asio::buffer(...) in /asio/buffer.hpp
		 * @return : Number of bytes written from the data. If an error occurred,
		 *           this will be less than the sum of the data size.
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * Note : If this function is called in communication thread, it will degenerates into async_send
		 *        and the return value is 0(success) or -1(failure).
		 * use like this : std::string m; send(std::move(m)); can reducing memory allocation.
		 * PodType * : send("abc");
		 * PodType (&data)[N] : double m[10]; send(m);
		 * std::array<PodType, N> : std::array<int,10> m; send(m);
		 * std::vector<PodType, Allocator> : std::vector<float> m; send(m);
		 * std::basic_string<Elem, Traits, Allocator> : std::string m; send(m);
		 */
		template<class DataT>
		inline std::size_t send(DataT&& data)
		{
			derived_t& derive = static_cast<derived_t&>(*this);

			std::future<std::pair<error_code, std::size_t>> future = derive.async_send(
				std::forward<DataT>(data), asio::use_future);

			// Whether we run on the strand
			if (derive.io().strand().running_in_this_thread())
			{
				std::future_status status = future.wait_for(std::chrono::nanoseconds(0));

				// async_send failed :
				// beacuse current thread is the io_context thread, so the data must be sent in the future,
				// so if the "status" is ready, it means that the data must have failed to be sent.
				if (status == std::future_status::ready)
				{
					set_last_error(future.get().first);
					return std::size_t(-1);
				}
				// async_send in_progress.
				else
				{
					set_last_error(asio::error::in_progress);
					return std::size_t(0);
				}
			}

			std::pair<error_code, std::size_t> pair = future.get();

			set_last_error(pair.first);

			return pair.second;
		}

		/**
		 * @function : Synchronous send data
		 * @return : Number of bytes written from the data. If an error occurred,
		 *           this will be less than the sum of the data size.
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * Note : If this function is called in communication thread, it will degenerates into async_send
		 *        and the return value is 0(success) or -1(failure).
		 * PodType * : send("abc");
		 */
		template<class CharT, class Traits = std::char_traits<CharT>>
		inline typename std::enable_if_t<
			std::is_same_v<detail::remove_cvref_t<CharT>, char> ||
			std::is_same_v<detail::remove_cvref_t<CharT>, wchar_t> ||
			std::is_same_v<detail::remove_cvref_t<CharT>, char16_t> ||
			std::is_same_v<detail::remove_cvref_t<CharT>, char32_t>, std::size_t> send(CharT * s)
		{
			derived_t& derive = static_cast<derived_t&>(*this);

			return derive.send(s, s ? Traits::length(s) : 0);
		}

		/**
		 * @function : Synchronous send data
		 * @return : Number of bytes written from the data. If an error occurred,
		 *           this will be less than the sum of the data size.
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * Note : If this function is called in communication thread, it will degenerates into async_send
		 *        and the return value is 0(success) or -1(failure).
		 * PodType (&data)[N] : double m[10]; send(m,5);
		 */
		template<class CharT, class SizeT>
		inline typename std::enable_if_t<std::is_integral_v<detail::remove_cvref_t<SizeT>>, std::size_t>
			send(CharT* s, SizeT count)
		{
			derived_t& derive = static_cast<derived_t&>(*this);

			return derive.send(derive._data_persistence(s, count));
		}

	protected:
	};
}

#endif // !__ASIO2_SEND_COMPONENT_HPP__
