#include "router.hh"

#include <iostream>
#include <cmath>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    // TODO: difference between std::make_tuple and std::tuple<.., .., ., ..>{} for initializer
    _routing_table.emplace_back(std::make_tuple(route_prefix, prefix_length, next_hop, interface_num));

    return;
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    const uint32_t &dst_ip_address = dgram.header().dst;

    auto entry = find_longest_prefix_match(dst_ip_address);

    if (entry == _routing_table.end())
    {
        sponge_log(LOG_INFO, "no entry for this dst ip in the routing table");
        return;
    }
    else
    {
        // prevent uint8(0) -1
        if (dgram.header().ttl <= 1)
            return;
        else
            dgram.header().ttl--;

        // TODO: need to ask the reference thing. Do I use too much reference?
        AsyncNetworkInterface &async_interface = interface(std::get<3>(*entry));
        if (std::get<2>(*entry).has_value())
            async_interface.send_datagram(dgram, std::get<2>(*entry).value());
        else    // means the router is connected to the destination network, don't need to transfer to other router
            async_interface.send_datagram(dgram, Address::from_ipv4_numeric(dgram.header().dst));

        return;
    }

    return;
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}

std::vector<std::tuple<uint32_t, uint8_t, std::optional<Address>, size_t>>::iterator Router::find_longest_prefix_match(
    const uint32_t &ip) {
    
    auto find = _routing_table.end();
    int match_length = -1;

    for (auto it = _routing_table.begin(); it != _routing_table.end(); it++) {
        int match_length_tmp = match_size(ip, std::get<0>(*it), std::get<1>(*it));
        if (match_length_tmp > match_length)
        {
            match_length = match_length_tmp;
            find = it;
        }
    }

    return find;
}

size_t Router::match_size(const uint32_t &ip, const uint32_t route_prefix, const uint8_t prefix_length) {
    uint32_t res = ip ^ route_prefix;

    int match_length = 32 - ceil(log2(res)+0.0001);

    if (route_prefix == 0)
    {
        return 0;
    }
    else
    {
        return match_length >= prefix_length ? match_length : 0;
    }
}