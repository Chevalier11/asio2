#ifndef ASIO2_USE_SSL
#define ASIO2_USE_SSL
#endif

//#include <asio2/asio2.hpp>
#include <asio2/tcp/tcps_client.hpp>
#include <iostream>

// how to use the match_role, see : https://blog.csdn.net/zhllxt/article/details/104772948

// the byte 1    head   (1 bytes) : #
// the byte 2    length (1 bytes) : the body length
// the byte 3... body   (n bytes) : the body content
using buffer_iterator = asio::buffers_iterator<asio::streambuf::const_buffers_type>;
std::pair<buffer_iterator, bool> match_role(buffer_iterator begin, buffer_iterator end)
{
	buffer_iterator i = begin;
	while (i != end)
	{
		if (*i != '#')
			return std::pair(begin, true); // head character is not #, return and kill the client

		i++;
		if (i == end) break;

		int length = std::uint8_t(*i); // get content length

		i++;
		if (i == end) break;

		if (end - i >= length)
			return std::pair(i + length, true);

		break;
	}
	return std::pair(begin, false);
}

int main()
{
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64) || defined(_WINDOWS_)
	// Detected memory leaks on windows system
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	std::string_view host = "127.0.0.1";
	std::string_view port = "8001";

	asio2::tcps_client client;

	client.set_verify_mode(asio::ssl::verify_peer);
	client.set_cert_file("../../cert/ca.crt", "../../cert/client.crt", "../../cert/client.key", "123456");

	client.start_timer(1, std::chrono::seconds(3), []()
	{
		std::cout << "timer was triggered in thread : " << std::this_thread::get_id() << std::endl;
	});

	client.bind_connect([&](asio::error_code ec)
	{
		if (asio2::get_last_error())
			printf("connect failure : %d %s\n", asio2::last_error_val(), asio2::last_error_msg().c_str());
		else
			printf("connect success : %s %u\n", client.local_address().c_str(), client.local_port());

		// connect success, send data
		if (!ec)
		{
			std::string s;
			s += '#';
			s += char(1);
			s += 'a';

			client.async_send(s);
		}

	}).bind_disconnect([](asio::error_code ec)
	{
		printf("disconnect : %d %s\n", ec.value(), ec.message().c_str());
	}).bind_recv([&](std::string_view sv)
	{
		printf("recv : %u %.*s\n", (unsigned)sv.size(), (int)sv.size(), sv.data());

		std::string s;
		s += '#';
		uint8_t len = uint8_t(100 + (std::rand() % 100));
		s += char(len);
		for (uint8_t i = 0; i < len; i++)
		{
			s += (char)((std::rand() % 26) + 'a');
		}

		// a data is split into two sends, but the another side received is a complete piece of data
		client.async_send(s.substr(0, s.size() / 2), []() {});

		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		client.async_send(s.substr(s.size() / 2), [](std::size_t bytes_sent) {std::ignore = bytes_sent; });

	});

	client.async_start(host, port, match_role);

	while (std::getchar() != '\n');

	return 0;
}
