#ifndef SERVICE_MDNS_H
#define SERVICE_MDNS_H

#include <ESPmDNS.h>
#include <config.h>

class MDNSService
{
public:
    explicit MDNSService();
    virtual ~MDNSService() {}

    static MDNSService*           getInstance() { return s_instance; }

    void loop();
    void addFriendlyName(String name);

private:
    static MDNSService*           s_instance;
    Config*                       m_config = Config::getInstance();
};

#endif