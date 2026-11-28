# ESP-IDF 6.1 uses LWIP_NETIF_TX_SINGLE_PBUF, which requires TCP_OVERSIZE.
# Each small TCP write can therefore allocate one full MSS. The single writer
# checks select() before every <=66-byte write. api_msg.c removes writability
# at two queued pbufs and restores it below two: at most two data pbufs per
# session, without changing SDK sources or introducing a transport ACK protocol.
# Four is lwIP's minimum queue ceiling for a two-MSS send window; the lower
# select threshold, not that ceiling, bounds this application's outstanding data.
idf_component_get_property(raven_lwip lwip COMPONENT_LIB)
target_compile_definitions(${raven_lwip} PUBLIC
    TCP_SND_QUEUELEN=4 TCP_SNDQUEUELOWAT=2 TCP_SNDLOWAT=128)
