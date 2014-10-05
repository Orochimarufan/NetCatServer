// Copyright (c) 2014 Taeyeon Mori <orochimarufan.x3@gmail.com>
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <iostream>

#include <cstring>
#include <string>
#include <algorithm>
#include <cstdio>
#include <regex>
#include <array>
#include <memory>
#include <sstream>

#include "cmdparser.h"
#include "sd-daemon.h"

using namespace std;


#define PASS_IN 1
#define PASS_OUT 2
#define PASS_ERR 4

CmdParser::ArgumentMap parse_argv(int argc, const std::vector<std::string> &exec_args);
void exec_pass_fd(int fd, int pass, char **exec_argv);


CmdParser::ArgumentMap parse_argv(int argc, char **argv)
{
    CmdParser::Parser parser;
    CmdParser::ArgumentMap args;

    // Help
    parser.newSwitch("help");
    parser.addFlag("help", 'h');
    parser.addDocumentation("help", "Show this help and exit");
    parser.setTerminal("help");

    // Systemd
    parser.newSwitch("systemd");
    parser.addDocumentation("systemd", "Use systemd socket activation");

    // Port
    parser.newOption("port", 7994l);
    parser.addFlag("port", 'p');
    parser.addDocumentation("port", "The port to listen on");

    // Address
    parser.newOption("bind");
    parser.addFlag("bind", 'b');
    parser.addDocumentation("bind", "Bind to address", "<addr>");

    // Ip version
    parser.newSwitch("ipv6");
    parser.addFlag("ipv6", '6');
    parser.addDocumentation("ipv6", "Use IPv6");

    // Fd passing
    parser.newSwitch("stdin");
    parser.addFlag("stdin", 'i');
    parser.addDocumentation("stdin", "Pass the standard input stream");
    parser.newSwitch("stdout");
    parser.addFlag("stdout", 'o');
    parser.addDocumentation("stdout", "Pass the standard output stream");
    parser.newSwitch("stderr");
    parser.addFlag("stderr", 'e');
    parser.addDocumentation("stderr", "Pass the standard error stream");

    parser.newArgument("exec", CmdParser::Variant::required);
    parser.addDocumentation("exec", "The program command line");

    try {
        args = parser.parse(argc, argv);
    } catch (CmdParser::ParsingError &e) {
        cout << "Error: " << e.what() << endl;
        cout << parser.compileUsage(argv[0]) << endl;
        exit(1);
    }

    if (args["help"].toBool())
    {
        cout << parser.compileHelp(argv[0]) << endl;
        exit(0);
    }

    return args;
}

union sockaddr_inet {
    short family;
    sockaddr_in in;
    sockaddr_in6 in6;
};

char *peername(const sockaddr_inet &peer)
{
    static char straddr[INET6_ADDRSTRLEN+3] = "[";

        inet_ntop(peer.family, &peer.in.sin_addr, straddr+1, sizeof(straddr-1));

        if (peer.family == AF_INET6)
        {
            strncpy(straddr + strlen(straddr), "]", 2);
            return straddr;
        }
        else
            return straddr+1;
}

struct Client
{
    static std::unique_ptr<Client> accept(int fd, int pass, const std::vector<std::string> &exec_argv)
    {
        sockaddr_inet sa;
        socklen_t sa_size = sizeof(sa);
        int sock = ::accept(fd, reinterpret_cast<sockaddr*>(&sa), &sa_size);

        if (sock < 0)
            return nullptr;

        return std::unique_ptr<Client>(new Client(sock, sa, pass, exec_argv));
    }

    int fd;
    sockaddr_inet peer;
    int pass;
    const std::vector<std::string> &exec_argv;
    std::vector<char*> argv;

    char *peername()
    {
        return ::peername(peer);
    }

    Client(int fd, const sockaddr_inet &peer, int pass, const std::vector<std::string> &exec_argv) :
        fd(fd), peer(peer),  pass(pass), exec_argv(exec_argv)
    {
    }

    int start()
    {
        int pid;
        if ((pid = fork()) != 0)
            return pid;

        run();
        exit(0);
    }

    // Replace vars
    static std::regex re;

    template <typename T>
    std::string ref_var(const T &var)
    {
        if (var[1] == "a")
            return peername();
        if (var[1] == "p")
        {
            std::ostringstream s;
            s << getpid();
            return s.str();
        }
        return var.str();
    }

    char *regex_replace_var(const std::string &str)
    {
        auto i = std::sregex_iterator(str.begin(), str.end(), re);
        auto i_end = decltype(i)();
        auto last_i = i;

        if (i == i_end)
            return const_cast<char*>(str.c_str());

        std::string res;
        for (; i != i_end; ++i)
        {
            //std::copy(i->prefix().first, i->prefix().second, res);
            res += i->prefix().str();
            res += ref_var(*i);
            last_i = i;
        }
        //std::copy(last_i->suffix().first, last_i->suffix().second, res);
        res += last_i->suffix().str();

        char *ret = new char[res.size() + 1];
        std::strncpy(ret, res.c_str(), res.size() + 1);
        return ret;
    }

    void prepare_argv()
    {
        argv.reserve(exec_argv.size() + 1);
        std::transform(exec_argv.begin(), exec_argv.end(), std::back_inserter(argv),
                       [this](const std::string &str){return this->regex_replace_var(str);}
                       );
        argv.push_back(NULL);
    }

    void __attribute__((noreturn)) run()
    {
        prepare_argv();

        cerr << "\033[36m[\033[35m" << getpid() << "\033[36m] Calling: \033[35m";
        cerr << argv[0];
        for (size_t i=1; i<argv.size()-1; ++i)
            cerr << " " << argv[i];
        cerr << "\033[0m" << endl;

        dup2(2, 200);
        fcntl(200, F_SETFD, FD_CLOEXEC);

        if (pass & PASS_IN)
            dup2(fd, 0);
        if (pass & PASS_OUT)
            dup2(fd, 1);
        if (pass & PASS_ERR)
            dup2(fd, 2);

        execvp(argv[0], argv.data());

        // restore stderr
        dup2(200, 2);

        cerr << "\033[31mError: ";
        perror("exec");
        cerr << "\033[0m";

        exit(1);
    }
};

std::regex Client::re("\\%([^\\%])");

int sock_fd = -1;
std::unordered_map<int, std::unique_ptr<Client>> pid_map;

void sigint_handler(int)
{
    cerr << "\033[31mCaught SIGINT. Shutting down.\033[0m" << endl;
    if (sock_fd >= 0)
    {
        close(sock_fd);
        sock_fd = -1;
    }
    exit(2);
}

void sigchld_handler(int, siginfo_t *si, void *)
{
    auto it = pid_map.find(si->si_pid);
    if (it == pid_map.end())
        cerr<< "\033[31m Unknown Connection lost: [\033[35m" << si->si_pid << "\033[31m]\033[0m" << endl;
    else
    {
        // HACK to move item from stl container
        std::unique_ptr<Client> c (std::move(it->second));
        pid_map.erase(it);

        cerr << "\033[36mConnection lost: \033[35m" << c->peername() << "\033[36m [\033[35m"
                << si->si_pid << "\033[36m]\033[0m" << endl;
    }
}

int main(int argc, char **argv)
{
    cerr << "\033[32mThis is \033[33mNetCatServer 1.0 \033[34m(c) 2014 Taeyeon Mori" << endl;
    cerr << "\033[32mThis program comes with \033[31mABSOLUTELY NO WARRANTY\033[32m.\033[0m" << endl;
    CmdParser::ArgumentMap args = parse_argv(argc, argv);

    // Parse exec line
    std::vector<std::string> exec_argv = CmdParser::splitArgs(args["exec"].toString());
    int pass = (args["stdin"].toBool() ? PASS_IN : 0) | (args["stdout"].toBool() ? PASS_OUT : 0) | (args["stderr"].toBool() ? PASS_ERR : 0);

    cerr << "\033[36mArgv: \033[35m['";
    cerr << exec_argv[0];
    for (size_t i=1; i<exec_argv.size(); ++i)
        cerr << "', '" << exec_argv[i];
    cerr << "']\033[0m" << endl;

    // Socket
    int fd;
    if (args["systemd"].toBool())
    {
        cerr << "\033[36mGetting socket from systemd...\033[0m" << endl;
        int n = sd_listen_fds(1);
        if (n > 1)
        {
            cerr << "\033[31mToo many fds received!\033[0m" << endl;
            exit(1);
        }
        else if (n < 1)
        {
            cerr << "\033[31mNo fds received. Check your systemd unit!\033[0m" << endl;
            exit(1);
        }
        fd = SD_LISTEN_FDS_START + 0;

        sockaddr_inet sa;
        socklen_t sa_size = sizeof(sa);
        getsockname(fd, reinterpret_cast<sockaddr*>(&sa), &sa_size);
        cerr << "\033[36mBound to \033[35m" << peername(sa) << ":" << ntohs(sa.in.sin_port) << "\033[0m" << endl;
    }
    else
    {
        cerr << "\033[36mOpening Listening Socket...\033[0m" << endl;

        std::string addrs = args["bind"].toString();
        if (!addrs.compare(0, 1, "[") && !addrs.compare(addrs.size()-1, 1, "]"))
        {
            args["ipv6"] = true;
            addrs = addrs.substr(1, addrs.size()-2);
        }

        sockaddr_inet addr;
        if (args["ipv6"].toBool())
        {
            addr.family = AF_INET6;
            addr.in6.sin6_addr = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
            addr.in6.sin6_port = htons((unsigned)args["port"].toNumber());
            if (!addrs.empty())
                inet_pton(AF_INET6, addrs.c_str(), &addr.in6.sin6_addr);
        }
        else
        {
            addr.in.sin_family = AF_INET;
            addr.in.sin_addr.s_addr = htonl(INADDR_ANY);
            addr.in.sin_port = htons((unsigned)args["port"].toNumber());
            if (!addrs.empty())
                inet_pton(AF_INET, addrs.c_str(), &addr.in.sin_addr);
        }

        fd = socket(addr.family, SOCK_STREAM, 0);
        fcntl(fd, F_SETFD, FD_CLOEXEC);

        if (fd < 0)
        {
            cerr << "\033[31mError: ";
            perror("socket");
            cerr << "\033[0m";
            exit(1);
        }


        if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
        {
            cerr << "\033[31mError: ";
            perror("bind");
            cerr << "\033[0m";
            exit(1);
        }

        if (listen(fd, 5) == -1)
        {
            cerr << "\033[31mError: ";
            perror("listen");
            cerr << "\033[0m";
            exit(1);
        }

        cerr << "\033[36mBound to \033[35m" << peername(addr) << ":" << ntohs(addr.in.sin_port) << "\033[0m" << endl;
    }

    // Signals
    struct sigaction sa;
    sock_fd = fd;

    sa.sa_handler = &sigint_handler;
    sigaction(SIGINT, &sa, 0);

    sa.sa_handler = NULL;
    sa.sa_sigaction = &sigchld_handler;
    sa.sa_flags = SA_NOCLDSTOP | SA_SIGINFO;
    sigaction(SIGCHLD, &sa, 0);

    cerr << "\033[36mNow accepting connections.\033[0m" << endl;
    while (true)
    {
        std::unique_ptr<Client> client = Client::accept(fd, pass, exec_argv);

        if (client == nullptr)
        {
            if (errno == EINTR)
                continue;
            cerr << "\033[31mError: ";
            perror("accept");
            cerr << "\033[0m";
            continue;
        }

        cerr << "\033[36mConnected: \033[35m" << client->peername() << "\033[0m";

        int pid = client->start();

        cerr << " \033[36m[\033[35m" << pid << "\033[36m]\033[0m" << endl;

        close(client->fd);
        pid_map.emplace(pid, std::move(client));
    }
}
