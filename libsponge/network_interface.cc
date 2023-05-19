#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "log.h"

#include <iostream>
#include <algorithm>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

static bool ethernet_address_is_all_zero(const EthernetAddress &val)
{
    bool res = 1;
    for (auto it = val.begin(); it != val.end(); it++)
    {
        if ((*it) != static_cast<std::uint8_t>(0))
        {
            res = 0;
            break;
        }
    }
    return res;
}

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address), _timer(0) {
    _timer.set_start(true);
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    auto found = _addr_mapping.find(next_hop_ip);

    if (found == _addr_mapping.end())   // not found in the mapping
    {
        // insert into map
        EthernetAddress tmp = {};
        _addr_mapping.insert({next_hop_ip, std::make_pair(tmp, 0)});
        // put into the queue
        _internet_datagram_out.push_back(std::make_pair(dgram, next_hop));
        // send ARP request for ethernet address of this ip
        send_ARP_datagram_request(next_hop);
    }
    else
    {
        const EthernetAddress &next_hop_ethernet_addr = (*found).second.first;
        int &life_time = (*found).second.second;
        if (ethernet_address_is_all_zero(next_hop_ethernet_addr)) // this ethernet address is actually empty
        {
            _internet_datagram_out.push_back(std::make_pair(dgram, next_hop));
            if (life_time < -ARP_RETX_TIMEOUT)
            {
                sponge_log(LOG_INFO, "ARP request timeout. resent.");
                send_ARP_datagram_request(next_hop);
                // update the latest life time
                life_time = 0;
            }
        }
        else    // found the the ethernet address in the mapping, so send IP datagram
        {
            send_IP_datagram(dgram, next_hop_ethernet_addr);
        }
    }

    return;
}

void NetworkInterface::send_IP_datagram(const InternetDatagram &dgram, const EthernetAddress &next_hop_ethernet_addr)
{
    // construct ethernet frame with ip datagram inside
    EthernetFrame eframe;
    eframe.payload() = dgram.serialize();
    eframe.header().src = _ethernet_address;
    eframe.header().dst = next_hop_ethernet_addr;
    eframe.header().type = EthernetHeader::TYPE_IPv4;
    _frames_out.push(eframe);
    return;
}

void NetworkInterface::send_ARP_datagram_request(const Address &next_hop)
{
    // construct ARP message for request
    ARPMessage arpmsg;
    arpmsg.sender_ethernet_address = _ethernet_address;
    arpmsg.sender_ip_address = _ip_address.ipv4_numeric();
    arpmsg.target_ip_address = next_hop.ipv4_numeric();
    arpmsg.opcode = ARPMessage::OPCODE_REQUEST;

    // construct ethernet frame with ARP message inside
    EthernetFrame eframe;
    eframe.payload() = arpmsg.serialize();
    eframe.header().src = _ethernet_address;
    eframe.header().dst = ETHERNET_BROADCAST;
    eframe.header().type = EthernetHeader::TYPE_ARP;
    _frames_out.push(eframe);

    return;
}

void NetworkInterface::send_ARP_datagram_reply(const uint32_t &next_hop)
{
    // construct ARP message for reply
    ARPMessage arpmsg;
    arpmsg.sender_ethernet_address = _ethernet_address;
    arpmsg.sender_ip_address = _ip_address.ipv4_numeric();
    if (auto found = _addr_mapping.find(next_hop); found != _addr_mapping.end())
        arpmsg.target_ethernet_address = (*found).second.first;
    else
        sponge_log(LOG_ERR, "ethernet address is expected to be found in map, but not");
    arpmsg.target_ip_address = next_hop;
    arpmsg.opcode = ARPMessage::OPCODE_REPLY;

    // construct ethernet frame with ARP message inside
    EthernetFrame eframe;
    eframe.payload() = arpmsg.serialize();
    eframe.header().src = _ethernet_address;
    eframe.header().dst = arpmsg.target_ethernet_address;
    eframe.header().type = EthernetHeader::TYPE_ARP;
    _frames_out.push(eframe);

    return;
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != ETHERNET_BROADCAST && frame.header().dst != _ethernet_address)
    {
        sponge_log(LOG_INFO, "the frame destination is neither broadcast nor my ethernet address");
        return {};
    }

    if (frame.header().type == EthernetHeader::TYPE_IPv4)
    {
        return recv_IP_datagram(frame);
    }
    else if (frame.header().type == EthernetHeader::TYPE_ARP)
    {
        recv_ARP_datagram(frame);
        return {};
        // TODO: I want to check if ARP datagram is request or reply datagram in code block here
    }
    else
    {
        sponge_log(LOG_ERR, "ethernet frame header type is unknown");
        return {};
    }
}

std::optional<InternetDatagram> NetworkInterface::recv_IP_datagram(const EthernetFrame &frame)
{
    InternetDatagram dgram;
    if (dgram.parse(frame.payload()) == ParseResult::NoError)
    {
        return dgram;
    }
    else
    {
        sponge_log(LOG_ERR, "error when parse internet datagram");
        return {};
    }
}

void NetworkInterface::recv_ARP_datagram(const EthernetFrame &frame)
{
    ARPMessage arpmsg;
    if (arpmsg.parse(frame.payload()) == ParseResult::NoError)
    {
        if (arpmsg.opcode == ARPMessage::OPCODE_REQUEST)
        {
            // if the source addr is not in the map, insert it
            if (auto found = _addr_mapping.find(arpmsg.sender_ip_address); found == _addr_mapping.end())
            {
                _addr_mapping.insert({arpmsg.sender_ip_address, std::make_pair(arpmsg.sender_ethernet_address, 0)});
            }
            
            // if request my own ip, send reply
            if (arpmsg.target_ip_address == _ip_address.ipv4_numeric())
            {
                send_ARP_datagram_reply(arpmsg.sender_ip_address);
            }
        }
        else if (arpmsg.opcode == ARPMessage::OPCODE_REPLY)
        {
            // if the src addr is not in the map, insert it
            if (auto search = _addr_mapping.find(arpmsg.sender_ip_address); search == _addr_mapping.end())
            {
                _addr_mapping.insert({arpmsg.sender_ip_address, std::make_pair(arpmsg.sender_ethernet_address, 0)});
            }
            else
            {
                (*search).second.first = arpmsg.sender_ethernet_address;
                (*search).second.second = 0;
            }

            // send the internet datagram vector
            int sent = 0;
            for (auto it = _internet_datagram_out.begin(); it != _internet_datagram_out.end();)
            {
                if ((*it).second.ipv4_numeric() == arpmsg.sender_ip_address)
                {
                    send_datagram((*it).first, (*it).second);
                    sent++;
                    // get rid from vector after sent
                    it = _internet_datagram_out.erase(it);
                }
                else
                {
                    it++;
                }
            }
            if (sent == 0)
            {
                sponge_log(LOG_INFO, "get ARP reply datagram, but sent no IP datagram");
            }
        }
        else
        {
            sponge_log(LOG_ERR, "unknown opcode in ARP message");
        }
        
        return;
    }
    else
    {
        sponge_log(LOG_ERR, "error when parse arp message");
        return;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick)
{
    _timer.update_time_by_last_time_passed(ms_since_last_tick);

    for (auto it = _addr_mapping.begin(); it != _addr_mapping.end();)
    {
        // update time in address mapping and discard ones out of their life cycles (30 seconds/30000 ms)
        if (ethernet_address_is_all_zero((*it).second.first) == false)
        {
            (*it).second.second += ms_since_last_tick;
            // remove the one out of its life cycle
            if ((*it).second.second > LIFETIME_IN_ADDR_MAPPING)
            {
                it = _addr_mapping.erase(it);
            }
            else
            {
                it++;
            }
        }
        else
        {
            (*it).second.second -= ms_since_last_tick;
            it++;
        }
    }

    return;
}
