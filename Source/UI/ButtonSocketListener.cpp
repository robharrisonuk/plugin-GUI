/*
------------------------------------------------------------------

This file is part of the Open Ephys GUI
Copyright (C) 2014 Open Ephys

------------------------------------------------------------------

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "ControlPanel.h"
#include "ButtonSocketListener.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <thread>

/*!
 * \brief Implementation of ButtonSocketListener
 */
class ButtonSocketListenerImpl
{
public:
	SOCKET m_socket = INVALID_SOCKET;
	std::thread* m_thread;
	bool m_running = true;
	RecordButton* m_button;

#define SOCKET_PORT "1299"
#define LOCAL_HOST "127.0.0.1"

	struct Message
	{
		int		m_command_code;
		double	m_time;
	};


	//---------------------------------------------------

	ButtonSocketListenerImpl(RecordButton* button) : m_button(button)
	{
		m_thread = new std::thread(std::bind(&ButtonSocketListenerImpl::ThreadRun, this));
	}

	//---------------------------------------------------

	~ButtonSocketListenerImpl()
	{
		m_running = false;
		closesocket(m_socket);
		m_thread->join();
		delete m_thread;
	}

	//---------------------------------------------------

	double GetTimeSecs()
	{
#ifdef _WIN32
		LARGE_INTEGER count;
		LARGE_INTEGER frequency;

		QueryPerformanceCounter(&count);
		QueryPerformanceFrequency(&frequency);

		return (double)count.QuadPart / (double)frequency.QuadPart;
#else
		struct timespec time;
		clock_gettime(CLOCK_REALTIME, &time);

		double t = (double)(time.tv_sec + (double)time.tv_nsec / 1e9);
		return t;
#endif
	}

	void ThreadRun()
	{
		while (m_running)
		{
			// Connect.
			while (m_socket == INVALID_SOCKET && m_running)
			{
				Sleep(500);
				OutputDebugString("Connecting\n");
				Connect();
			}

			char recvbuf[512];
			char str[256];

			if (m_socket != INVALID_SOCKET && m_running)
			{
				OutputDebugString("Connected to button server\n");

				int result;

				do {
					result = recv(m_socket, recvbuf, 512, 0);
					if (result > 0)
					{
						if (result == sizeof(Message))
						{
							Message* msg = (Message*)recvbuf;

							if (msg->m_command_code == 0)
							{
								sprintf_s(str, "Time - %.3f ms\n", (GetTimeSecs() - msg->m_time) * 1000.0);
								OutputDebugString(str);

								m_button->StartRecording();
							}
							else if (msg->m_command_code == 1)
							{
								m_button->StopRecording();
							}
						}
					}
					else if (result == 0)
					{
						OutputDebugString("Connection closed\n");
					}
					else
					{
						sprintf_s(str, "recv failed with error: %d\n", WSAGetLastError());
						OutputDebugString(str);
					}

				} while (result > 0);

				closesocket(m_socket);
				WSACleanup();

				m_socket = INVALID_SOCKET;
			}
		}
	}

	bool Connect()
	{
		WSADATA wsaData;
		struct addrinfo hints;
		struct addrinfo* addresses;
		char str[256];

		int error = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (error != 0)
		{
			sprintf_s(str, "WSAStartup failed with error: %d\n", error);
			OutputDebugString(str);
			return false;
		}

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		error = getaddrinfo(LOCAL_HOST, SOCKET_PORT, &hints, &addresses);
		if (error != 0)
		{
			sprintf_s(str, "getaddrinfo failed with error: %d\n", error);
			OutputDebugString(str);
			WSACleanup();
			return false;
		}

		for (struct addrinfo* ptr = addresses; ptr != NULL; ptr = ptr->ai_next)
		{

			// Create a SOCKET for connecting to server
			m_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
			if (m_socket == INVALID_SOCKET)
			{
				sprintf_s(str, "socket failed with error: %ld\n", WSAGetLastError());
				OutputDebugString(str);
				WSACleanup();
				return false;
			}

			// Connect to server.
			error = connect(m_socket, ptr->ai_addr, (int)ptr->ai_addrlen);
			if (error == SOCKET_ERROR)
			{
				closesocket(m_socket);
				m_socket = INVALID_SOCKET;
				continue;
			}
			break;
		}

		freeaddrinfo(addresses);

		if (m_socket == INVALID_SOCKET) {
			OutputDebugString("Unable to connect to server!\n");
			WSACleanup();
			return false;
		}

		return true;
	}
};
#endif	// _WIN32


//-------------------------------------------------------

ButtonSocketListener::ButtonSocketListener(RecordButton* button)
{
#ifdef _WIN32
	m_impl = new ButtonSocketListenerImpl(button);
#else
	m_impl = nullptr;
#endif
}

//-------------------------------------------------------

ButtonSocketListener::~ButtonSocketListener()
{
#ifdef _WIN32
	delete m_impl;
#endif
}

