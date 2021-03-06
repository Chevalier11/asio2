/*
 * COPYRIGHT (C) 2017-2021, zhllxt
 *
 * author   : zhllxt
 * email    : 37792738@qq.com
 * 
 * Distributed under the GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
 * (See accompanying file LICENSE or see <http://www.gnu.org/licenses/>)
 */

#ifndef __ASIO2_UDP_SESSION_HPP__
#define __ASIO2_UDP_SESSION_HPP__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <asio2/base/detail/push_options.hpp>

#include <asio2/base/session.hpp>
#include <asio2/base/detail/linear_buffer.hpp>
#include <asio2/udp/impl/udp_send_op.hpp>
#include <asio2/udp/detail/kcp_util.hpp>
#include <asio2/udp/component/kcp_stream_cp.hpp>

namespace asio2::detail
{
	struct template_args_udp_session
	{
		static constexpr bool is_session = true;
		static constexpr bool is_client  = false;
		static constexpr bool is_server  = false;

		using socket_t    = asio::ip::udp::socket&;
		using buffer_t    = detail::empty_buffer;
		using send_data_t = std::string_view;
		using recv_data_t = std::string_view;
	};

	ASIO2_CLASS_FORWARD_DECLARE_BASE;
	ASIO2_CLASS_FORWARD_DECLARE_UDP_BASE;
	ASIO2_CLASS_FORWARD_DECLARE_UDP_SERVER;
	ASIO2_CLASS_FORWARD_DECLARE_UDP_SESSION;

	template<class derived_t, class args_t = template_args_udp_session>
	class udp_session_impl_t
		: public session_impl_t<derived_t, args_t>
		, public udp_send_op   <derived_t, args_t>
	{
		ASIO2_CLASS_FRIEND_DECLARE_BASE;
		ASIO2_CLASS_FRIEND_DECLARE_UDP_BASE;
		ASIO2_CLASS_FRIEND_DECLARE_UDP_SERVER;
		ASIO2_CLASS_FRIEND_DECLARE_UDP_SESSION;

	public:
		using super = session_impl_t    <derived_t, args_t>;
		using self  = udp_session_impl_t<derived_t, args_t>;

		using key_type = asio::ip::udp::endpoint;

		using buffer_type = typename args_t::buffer_t;
		using send_data_t = typename args_t::send_data_t;
		using recv_data_t = typename args_t::recv_data_t;

	public:
		/**
		 * @constructor
		 */
		explicit udp_session_impl_t(
			session_mgr_t<derived_t>                 & sessions,
			listener_t                               & listener,
			io_t                                     & rwio,
			std::size_t                                init_buf_size,
			std::size_t                                max_buf_size,
			asio2::buffer_wrap<asio2::linear_buffer> & buffer,
			typename args_t::socket_t                  socket,
			asio::ip::udp::endpoint                  & endpoint
		)
			: super(sessions, listener, rwio, init_buf_size, max_buf_size, socket)
			, udp_send_op<derived_t, args_t>()
			, buffer_ref_     (buffer)
			, remote_endpoint_(endpoint)
			, wallocator_     ()
		{
			this->silence_timeout(std::chrono::milliseconds(udp_silence_timeout));
			this->connect_timeout(std::chrono::milliseconds(udp_connect_timeout));
		}

		/**
		 * @destructor
		 */
		~udp_session_impl_t()
		{
		}

	protected:
		/**
		 * @function : start this session for prepare to recv msg
		 */
		template<typename MatchCondition>
		inline void start(condition_wrap<MatchCondition> condition)
		{
			try
			{
			#if defined(ASIO2_ENABLE_LOG)
				this->is_stop_silence_timer_called_ = false;
				this->is_stop_connect_timeout_timer_called_ = false;
				this->is_disconnect_called_ = false;
			#endif

				state_t expected = state_t::stopped;
				if (!this->state_.compare_exchange_strong(expected, state_t::starting))
					asio::detail::throw_error(asio::error::already_started);

				std::shared_ptr<derived_t> this_ptr = this->derived().selfptr();

				this->derived()._do_init(this_ptr, condition);

				// First call the base class start function
				super::start();

				// if the match condition is remote data call mode,do some thing.
				this->derived()._rdc_init(condition);

				this->derived()._handle_connect(error_code{}, std::move(this_ptr), std::move(condition));
			}
			catch (system_error & e)
			{
				set_last_error(e);

				this->derived()._do_disconnect(e.code());
			}
		}

	public:
		/**
		 * @function : stop session
		 * note : this function must be noblocking,if it's blocking,maybe cause circle lock.
		 * You can call this function on the communication thread and anywhere to stop the session.
		 */
		inline void stop()
		{
			this->derived()._do_disconnect(asio::error::operation_aborted);
		}

	public:
		/**
		 * @function : get the remote address
		 */
		inline std::string remote_address()
		{
			try
			{
				return this->remote_endpoint_.address().to_string();
			}
			catch (system_error & e) { set_last_error(e); }
			return std::string();
		}

		/**
		 * @function : get the remote port
		 */
		inline unsigned short remote_port()
		{
			try
			{
				return this->remote_endpoint_.port();
			}
			catch (system_error & e) { set_last_error(e); }
			return static_cast<unsigned short>(0);
		}

		/**
		 * @function : get this object hash key,used for session map
		 */
		inline const key_type & hash_key() const
		{
			return this->remote_endpoint_;
		}

		/**
		 * @function : get the kcp pointer, just used for kcp mode
		 * default mode : ikcp_nodelay(kcp, 0, 10, 0, 0);
		 * generic mode : ikcp_nodelay(kcp, 0, 10, 0, 1);
		 * fast    mode : ikcp_nodelay(kcp, 1, 10, 2, 1);
		 */
		inline kcp::ikcpcb* kcp()
		{
			return (this->kcp_ ? this->kcp_->kcp_ : nullptr);
		}

	protected:
		template<typename MatchCondition>
		inline void _do_init(std::shared_ptr<derived_t> this_ptr, condition_wrap<MatchCondition> condition)
		{
			detail::ignore_unused(this_ptr, condition);

			// reset the variable to default status
			this->reset_connect_time();
			this->update_alive_time();
		}

		template<typename MatchCondition>
		inline void _handle_connect(const error_code& ec, std::shared_ptr<derived_t> this_ptr,
			condition_wrap<MatchCondition> condition)
		{
			detail::ignore_unused(ec);

			if constexpr (std::is_same_v<typename condition_wrap<MatchCondition>::condition_type, use_kcp_t>)
			{
				// step 3 : server recvd syn from client (the first_ is syn)
				// Check whether the first_ packet is SYN handshake
				if (!kcp::is_kcphdr_syn(this->first_))
				{
					set_last_error(asio::error::address_family_not_supported);
					this->derived()._fire_handshake(this_ptr, asio::error::address_family_not_supported);
					this->derived()._do_disconnect(asio::error::address_family_not_supported);
					return;
				}
				this->kcp_ = std::make_unique<kcp_stream_cp<derived_t, args_t>>(this->derived(), this->io_);
				this->kcp_->_post_handshake(std::move(this_ptr), std::move(condition));
			}
			else
			{
				this->kcp_.reset();
				this->derived()._done_connect(ec, std::move(this_ptr), std::move(condition));
			}
		}

		template<typename MatchCondition>
		inline void _do_start(std::shared_ptr<derived_t> this_ptr, condition_wrap<MatchCondition> condition)
		{
			if constexpr (std::is_same_v<typename condition_wrap<MatchCondition>::condition_type, use_kcp_t>)
			{
				ASIO2_ASSERT(this->kcp_);

				if (this->kcp_)
					this->kcp_->send_fin_ = true;
			}

			this->derived()._join_session(std::move(this_ptr), std::move(condition));
		}

		inline void _handle_disconnect(const error_code& ec, std::shared_ptr<derived_t> this_ptr)
		{
			ASIO2_ASSERT(this->derived().io().strand().running_in_this_thread());

			set_last_error(ec);

			this->derived()._rdc_stop();

			this->derived()._do_stop(ec, std::move(this_ptr));
		}

		inline void _do_stop(const error_code& ec, std::shared_ptr<derived_t> this_ptr)
		{
			this->derived()._post_stop(ec, std::move(this_ptr));
		}

		inline void _post_stop(const error_code& ec, std::shared_ptr<derived_t> this_ptr)
		{
			// if we use asio::dispatch in server's _exec_stop, then we need:
			// put _kcp_stop in front of super::stop, othwise the super::stop will execute
			// "counter_ptr_.reset()", it will cause the udp server's _exec_stop is called,
			// and the _handle_stop is called, and the socket will be closed, then the 
			// _kcp_stop send kcphdr will failed.
			// but if we use asio::post in server's _exec_stop, there is no such problem.
			if (this->kcp_)
				this->kcp_->_kcp_stop();

			// call the base class stop function
			super::stop();

			// call CRTP polymorphic stop
			this->derived()._handle_stop(ec, std::move(this_ptr));
		}

		inline void _handle_stop(const error_code& ec, std::shared_ptr<derived_t> this_ptr)
		{
			detail::ignore_unused(ec, this_ptr);

			state_t expected = state_t::stopping;
			if (!this->state_.compare_exchange_strong(expected, state_t::stopped))
			{
				ASIO2_ASSERT(false);
			}
		}

		template<typename MatchCondition>
		inline void _join_session(std::shared_ptr<derived_t> this_ptr, condition_wrap<MatchCondition> condition)
		{
			this->sessions_.emplace(this_ptr, [this, this_ptr, condition = std::move(condition)]
			(bool inserted) mutable
			{
				if (inserted)
					this->derived()._start_recv(std::move(this_ptr), std::move(condition));
				else
					this->derived()._do_disconnect(asio::error::address_in_use);
			});
		}

		template<typename MatchCondition>
		inline void _start_recv(std::shared_ptr<derived_t> this_ptr, condition_wrap<MatchCondition> condition)
		{
			// to avlid the user call stop in another thread,then it may be socket_.async_read_some
			// and socket_.close be called at the same time
			asio::dispatch(this->io_.strand(), make_allocator(this->wallocator_,
			[this, this_ptr = std::move(this_ptr), condition = std::move(condition)]() mutable
			{
				using condition_type = typename condition_wrap<MatchCondition>::condition_type;

				// start the timer of check silence timeout
				this->derived()._post_silence_timer(this->silence_timeout_, this_ptr);

				if constexpr (std::is_same_v<condition_type, asio2::detail::use_kcp_t>)
					detail::ignore_unused(this_ptr, condition);
				else
					this->derived()._handle_recv(error_code{}, this->first_, this_ptr, std::move(condition));
			}));
		}

	protected:
		template<class Data, class Callback>
		inline bool _do_send(Data& data, Callback&& callback)
		{
			if (!this->kcp_)
				return this->derived()._udp_send_to(
					this->remote_endpoint_, data, std::forward<Callback>(callback));
			return this->kcp_->_kcp_send(data, std::forward<Callback>(callback));
		}

		template<class Data>
		inline send_data_t _rdc_convert_to_send_data(Data& data)
		{
			auto buffer = asio::buffer(data);
			return send_data_t{ reinterpret_cast<
				std::string_view::const_pointer>(buffer.data()),buffer.size() };
		}

		template<class Invoker>
		inline void _rdc_invoke_with_none(const error_code& ec, Invoker& invoker)
		{
			if (invoker)
				invoker(ec, send_data_t{}, recv_data_t{});
		}

		template<class Invoker>
		inline void _rdc_invoke_with_recv(const error_code& ec, Invoker& invoker, recv_data_t data)
		{
			if (invoker)
				invoker(ec, send_data_t{}, data);
		}

		template<class Invoker, class FnData>
		inline void _rdc_invoke_with_send(const error_code& ec, Invoker& invoker, FnData& fn_data)
		{
			if (invoker)
				invoker(ec, fn_data(), recv_data_t{});
		}

	protected:
		template<typename MatchCondition>
		inline void _handle_recv(const error_code& ec, std::string_view data,
			std::shared_ptr<derived_t>& this_ptr, condition_wrap<MatchCondition> condition)
		{
			detail::ignore_unused(ec);

			if (!this->is_started())
				return;

			this->update_alive_time();

			if constexpr (!std::is_same_v<typename condition_wrap<MatchCondition>::condition_type, use_kcp_t>)
			{
				this->derived()._fire_recv(this_ptr, std::move(data), condition);
			}
			else
			{
				if (data.size() == kcp::kcphdr::required_size())
				{
					if /**/ (kcp::is_kcphdr_fin(data))
					{
						this->kcp_->send_fin_ = false;
						this->derived()._do_disconnect(asio::error::connection_reset);
					}
					// Check whether the packet is SYN handshake
					// It is possible that the client did not receive the synack package, then the client
					// will resend the syn package, so we just need to reply to the syncack package directly.
					else if (kcp::is_kcphdr_syn(data))
					{
						// beacuse the server has handled the syn already, so this code will never executed.
						ASIO2_ASSERT(this->kcp_ && this->kcp_->kcp_);
						// step 4 : server send synack to client
						kcp::kcphdr hdr    = kcp::to_kcphdr(data);
						kcp::kcphdr synack = kcp::make_kcphdr_synack(this->kcp_->seq_, hdr.th_seq);
						error_code ed;
						this->kcp_->_kcp_send_hdr(synack, ed);
						if (ed)
							this->derived()._do_disconnect(ed);
					}
				}
				else
				{
					this->kcp_->_kcp_recv(this_ptr, data, this->buffer_ref_, condition);
				}
			}
		}

		template<typename MatchCondition>
		inline void _fire_recv(std::shared_ptr<derived_t>& this_ptr, std::string_view data,
			condition_wrap<MatchCondition>& condition)
		{
			this->listener_.notify(event_type::recv, this_ptr, data);

			this->derived()._rdc_handle_recv(this_ptr, data, condition);
		}

		inline void _fire_handshake(std::shared_ptr<derived_t>& this_ptr, error_code ec)
		{
			// the _fire_handshake must be executed in the thread 0.
			ASIO2_ASSERT(this->sessions().io().strand().running_in_this_thread());

			this->listener_.notify(event_type::handshake, this_ptr, ec);
		}

		template<typename MatchCondition>
		inline void _fire_connect(std::shared_ptr<derived_t>& this_ptr, condition_wrap<MatchCondition>& condition)
		{
			// the _fire_connect must be executed in the thread 0.
			ASIO2_ASSERT(this->sessions().io().strand().running_in_this_thread());

		#if defined(ASIO2_ENABLE_LOG)
			ASIO2_ASSERT(this->is_disconnect_called_ == false);
		#endif

			this->derived()._rdc_start(this_ptr, condition);

			this->listener_.notify(event_type::connect, this_ptr);
		}

		inline void _fire_disconnect(std::shared_ptr<derived_t>& this_ptr)
		{
			// the _fire_disconnect must be executed in the thread 0.
			ASIO2_ASSERT(this->sessions().io().strand().running_in_this_thread());

		#if defined(ASIO2_ENABLE_LOG)
			this->is_disconnect_called_ = true;
		#endif

			this->listener_.notify(event_type::disconnect, this_ptr);
		}

	protected:
		/**
		 * @function : get the recv/read allocator object refrence
		 */
		inline auto & rallocator() { return this->wallocator_; }
		/**
		 * @function : get the send/write allocator object refrence
		 */
		inline auto & wallocator() { return this->wallocator_; }

	protected:
		/// buffer
		asio2::buffer_wrap<asio2::linear_buffer>        & buffer_ref_;

		/// used for session_mgr's session unordered_map key
		asio::ip::udp::endpoint                           remote_endpoint_;

		/// The memory to use for handler-based custom memory allocation. used fo send/write.
		handler_memory<size_op<>, std::true_type>         wallocator_;

		std::unique_ptr<kcp_stream_cp<derived_t, args_t>> kcp_;

		/// first recvd data packet
		std::string_view                                  first_;

	#if defined(ASIO2_ENABLE_LOG)
		bool                                              is_disconnect_called_ = false;
	#endif
	};
}

namespace asio2
{
	template<class derived_t>
	class udp_session_t : public detail::udp_session_impl_t<derived_t, detail::template_args_udp_session>
	{
	public:
		using detail::udp_session_impl_t<derived_t, detail::template_args_udp_session>::udp_session_impl_t;
	};

	class udp_session : public udp_session_t<udp_session>
	{
	public:
		using udp_session_t<udp_session>::udp_session_t;
	};
}

#include <asio2/base/detail/pop_options.hpp>

#endif // !__ASIO2_UDP_SESSION_HPP__
