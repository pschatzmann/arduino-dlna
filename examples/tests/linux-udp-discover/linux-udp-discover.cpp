/*
        Simple Linux SSDP / DLNA discovery test

        - Binds a UDP socket to INADDR_ANY:1900 with SO_REUSEADDR/SO_REUSEPORT
        - Joins the SSDP multicast group 239.255.255.250
        - Sends an M-SEARCH request (ssdp:all)
        - Listens for replies and prints the raw reply plus parsed
   LOCATION/USN/ST

        Build via CMake in the project's build tree and run on a Linux machine.
*/

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <iostream>
#include <string>

static const char* MCAST_ADDR = "239.255.255.250";
static const int MCAST_PORT = 1900;

static std::string str_tolower(std::string s) {
  for (auto& c : s) c = tolower((unsigned char)c);
  return s;
}

static std::string header_value_ci(const std::string& payload,
                                   const char* key) {
  // case-insensitive search for key:" value"
  std::string pl = str_tolower(payload);
  std::string k = str_tolower(key);
  size_t pos = pl.find(k);
  if (pos == std::string::npos) return std::string();
  pos += k.size();
  // skip ':' and whitespace
  while (pos < pl.size() && (pl[pos] == ':' || isspace((unsigned char)pl[pos])))
    pos++;
  // read until CRLF
  size_t end = pl.find('\r', pos);
  if (end == std::string::npos) end = pl.find('\n', pos);
  if (end == std::string::npos) end = pl.size();
  // return original-case substring by mapping positions
  return payload.substr(pos, end - pos);
}

// create and bind the main socket to INADDR_ANY:1900 with optional SO_REUSEPORT
static int create_bound_socket(bool reuse_port) {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket");
    return -1;
  }
  int reuse = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    perror("setsockopt SO_REUSEADDR");
    // continue
  }
  if (reuse_port) {
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
      // some systems don't support SO_REUSEPORT; ignore error
    }
  }

  struct sockaddr_in bind_addr;
  memset(&bind_addr, 0, sizeof(bind_addr));
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  bind_addr.sin_port = htons(MCAST_PORT);
  if (bind(sock, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
    perror("bind");
    close(sock);
    return -1;
  }
  return sock;
}

// join multicast group either per-interface or single on INADDR_ANY
static void join_multicast_group(int sock, bool join_per_interface) {
  if (!join_per_interface) {
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(MCAST_ADDR);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) <
        0) {
      perror("setsockopt IP_ADD_MEMBERSHIP");
    }
    return;
  }

  struct ifaddrs* ifap = nullptr;
  if (getifaddrs(&ifap) != 0) {
    perror("getifaddrs for join_per_interface");
    return;
  }
  for (struct ifaddrs* ifa = ifap; ifa != nullptr; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr) continue;
    if (ifa->ifa_addr->sa_family != AF_INET) continue;
    struct sockaddr_in* sin = (struct sockaddr_in*)ifa->ifa_addr;
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(MCAST_ADDR);
    mreq.imr_interface.s_addr = sin->sin_addr.s_addr;
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) <
        0) {
      std::cerr << "IP_ADD_MEMBERSHIP failed for iface "
                << (ifa->ifa_name ? ifa->ifa_name : "?") << " -> "
                << strerror(errno) << "\n";
    } else {
      char addrbuf[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &sin->sin_addr, addrbuf, sizeof(addrbuf));
      std::cout << "Joined multicast on iface "
                << (ifa->ifa_name ? ifa->ifa_name : "?") << " addr=" << addrbuf
                << "\n";
    }
  }
  freeifaddrs(ifap);
}

// create an ephemeral socket bound to INADDR_ANY:0 (returns fd or -1)
static int create_ephemeral_socket() {
  int esock = socket(AF_INET, SOCK_DGRAM, 0);
  if (esock < 0) {
    perror("creating ephemeral socket");
    return -1;
  }
  int reuse_e = 1;
  setsockopt(esock, SOL_SOCKET, SO_REUSEADDR, &reuse_e, sizeof(reuse_e));
  struct sockaddr_in eb;
  memset(&eb, 0, sizeof(eb));
  eb.sin_family = AF_INET;
  eb.sin_addr.s_addr = htonl(INADDR_ANY);
  eb.sin_port = htons(0);
  if (bind(esock, (struct sockaddr*)&eb, sizeof(eb)) < 0) {
    perror("bind ephemeral");
    close(esock);
    return -1;
  }
  struct sockaddr_in actual;
  socklen_t al = sizeof(actual);
  if (getsockname(esock, (struct sockaddr*)&actual, &al) == 0) {
    char addrbuf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &actual.sin_addr, addrbuf, sizeof(addrbuf));
    std::cout << "Ephemeral socket bound to port " << ntohs(actual.sin_port)
              << " on addr " << addrbuf << "\n";
  }
  return esock;
}

// send M-SEARCH on each AF_INET interface (sets IP_MULTICAST_IF per iface)
static bool send_msearch_on_interfaces(int main_sock, int esock,
                                       bool use_ephemeral, const char* msearch,
                                       const struct sockaddr_in& dst) {
  struct ifaddrs* ifap2 = nullptr;
  bool sent_any = false;
  if (getifaddrs(&ifap2) == 0) {
    for (struct ifaddrs* ifa = ifap2; ifa != nullptr; ifa = ifa->ifa_next) {
      if (!ifa->ifa_addr) continue;
      if (ifa->ifa_addr->sa_family != AF_INET) continue;
      std::cout << "IP_MULTICAST_IF " << (ifa->ifa_name ? ifa->ifa_name : "?");
      struct sockaddr_in* sin = (struct sockaddr_in*)ifa->ifa_addr;
      char addrbuf[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &sin->sin_addr, addrbuf, sizeof(addrbuf));

      int target_sock = use_ephemeral && esock >= 0 ? esock : main_sock;
      if (setsockopt(target_sock, IPPROTO_IP, IP_MULTICAST_IF, &sin->sin_addr,
                     sizeof(sin->sin_addr)) < 0) {
        std::cerr << "IP_MULTICAST_IF failed for iface "
                  << (ifa->ifa_name ? ifa->ifa_name : "?")
                  << " addr=" << addrbuf << " -> " << strerror(errno) << "\n";
        continue;
      }
      ssize_t s = -1;
      if (target_sock >= 0) {
        s = sendto(target_sock, msearch, strlen(msearch), 0,
                   (struct sockaddr*)&dst, sizeof(dst));
      }
      if (s < 0) {
        std::cerr << "sendto on iface " << (ifa->ifa_name ? ifa->ifa_name : "?")
                  << " failed: " << strerror(errno) << "\n";
      } else {
        sent_any = true;
        std::cout << "Sent M-SEARCH (" << s << " bytes) on iface "
                  << (ifa->ifa_name ? ifa->ifa_name : "?")
                  << " addr=" << addrbuf << " -> " << MCAST_ADDR << ":"
                  << MCAST_PORT << "\n";
      }
    }
    freeifaddrs(ifap2);
  }
  return sent_any;
}

static void fallback_send(int main_sock, int esock, bool use_ephemeral,
                          const char* msearch, const struct sockaddr_in& dst,
                          bool sent_any) {
  ssize_t sent = -1;
  if (use_ephemeral) {
    int esock_try = socket(AF_INET, SOCK_DGRAM, 0);
    if (esock_try >= 0) {
      int reuse_e = 1;
      setsockopt(esock_try, SOL_SOCKET, SO_REUSEADDR, &reuse_e,
                 sizeof(reuse_e));
      sent = sendto(esock_try, msearch, strlen(msearch), 0,
                    (struct sockaddr*)&dst, sizeof(dst));
      close(esock_try);
    }
  } else {
    sent = sendto(main_sock, msearch, strlen(msearch), 0,
                  (struct sockaddr*)&dst, sizeof(dst));
  }
  if (sent < 0) {
    perror("sendto");
    if (!sent_any) {
      // nothing else we can do here
    }
  } else if (!sent_any) {
    std::cout << "Sent M-SEARCH (" << sent << " bytes) to " << MCAST_ADDR << ":"
              << MCAST_PORT << "\n";
  }
}

static void listen_for_replies(int recv_sock, int timeout_seconds) {
  struct timeval tv_start;
  gettimeofday(&tv_start, nullptr);
  while (true) {
    struct timeval now;
    gettimeofday(&now, nullptr);
    long elapsed = now.tv_sec - tv_start.tv_sec;
    if (elapsed >= timeout_seconds) break;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(recv_sock, &readfds);
    struct timeval tv;
    tv.tv_sec = timeout_seconds - elapsed;
    tv.tv_usec = 0;
    int rc = select(recv_sock + 1, &readfds, NULL, NULL, &tv);
    if (rc < 0) {
      if (errno == EINTR) continue;
      perror("select");
      break;
    }
    if (rc == 0) continue;
    if (FD_ISSET(recv_sock, &readfds)) {
      char buf[8192];
      struct sockaddr_in src;
      socklen_t src_len = sizeof(src);
      ssize_t r = recvfrom(recv_sock, buf, sizeof(buf) - 1, 0,
                           (struct sockaddr*)&src, &src_len);
      if (r < 0) {
        perror("recvfrom");
        continue;
      }
      buf[r] = '\0';
      char* p = buf;
      while (*p && isspace((unsigned char)*p)) p++;
      if (strncasecmp(p, "M-SEARCH", 8) == 0) {
        continue;
      }
      char srcbuf[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &src.sin_addr, srcbuf, sizeof(srcbuf));
      int srcport = ntohs(src.sin_port);
      std::string payload(buf);
      std::cout << "\n--- Reply from " << srcbuf << ":" << srcport << " (" << r
                << " bytes) ---\n";
      std::cout << payload << "\n";
      std::string location = header_value_ci(payload, "location");
      std::string usn = header_value_ci(payload, "usn");
      std::string st = header_value_ci(payload, "st");
      if (!location.empty()) std::cout << "LOCATION: " << location << "\n";
      if (!usn.empty()) std::cout << "USN: " << usn << "\n";
      if (!st.empty()) std::cout << "ST: " << st << "\n";
    }
  }
}

static void print_usage(const char* prog) {
  std::cout << "Usage: " << prog << " [options]\n"
            << "Options:\n"
            << "  --join-per-interface    Join multicast per-interface "
               "(default: off)\n"
            << "  --no-reuseport         Disable SO_REUSEPORT (default: "
               "reuseport on)\n"
            << "  --no-ephemeral         Do not use ephemeral source socket "
               "(default: use ephemeral)\n"
            << "  --help                 Show this help\n";
}

int main(int argc, char** argv) {
  bool reuse_port = true;           // default ON
  bool join_per_interface = false;  // default OFF
  bool use_ephemeral = true;        // keep current default

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--no-reuseport") == 0) {
      reuse_port = false;
    } else if (strcmp(argv[i], "--join-per-interface") == 0) {
      join_per_interface = true;
    } else if (strcmp(argv[i], "--no-ephemeral") == 0) {
      use_ephemeral = false;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      print_usage(argv[0]);
      return 0;
    } else {
      std::cerr << "Unknown option: " << argv[i] << "\n";
      print_usage(argv[0]);
      return 1;
    }
  }

  std::cout << "Options: reuse_port=" << (reuse_port ? "on" : "off")
            << " join_per_interface=" << (join_per_interface ? "on" : "off")
            << " use_ephemeral=" << (use_ephemeral ? "on" : "off") << "\n";

  int sock = create_bound_socket(reuse_port);
  if (sock < 0) return 1;

  join_multicast_group(sock, join_per_interface);

  struct sockaddr_in dst;
  memset(&dst, 0, sizeof(dst));
  dst.sin_family = AF_INET;
  dst.sin_addr.s_addr = inet_addr(MCAST_ADDR);
  dst.sin_port = htons(MCAST_PORT);

  const char* msearch =
      "M-SEARCH * HTTP/1.1\r\n"
      "HOST: 239.255.255.250:1900\r\n"
      "MAN: \"ssdp:discover\"\r\n"
      "MX: 2\r\n"
      "ST: urn:schemas-upnp-org:device:DimmableLight:1\r\n"
      "\r\n";

  int esock = -1;
  if (use_ephemeral) {
    esock = create_ephemeral_socket();
    if (esock < 0) {
      use_ephemeral = false;
    }
  }

  bool sent_any =  send_msearch_on_interfaces(sock, esock, use_ephemeral, msearch, dst);
  //fallback_send(sock, esock, use_ephemeral, msearch, dst, sent_any);

  const int timeout_seconds = 60;
  int recv_sock = use_ephemeral && esock >= 0 ? esock : sock;
  listen_for_replies(recv_sock, timeout_seconds);

  std::cout << "\nFinished listening for replies (" << timeout_seconds
            << "s).\n";
  if (esock >= 0) close(esock);
  close(sock);
  return 0;
}