#ifndef SERVICE_MDNS_H
#define SERVICE_MDNS_H

#include <ESPmDNS.h>

class MDNSService
{
public:
    explicit MDNSService();
    virtual ~MDNSService() {}

    static MDNSService*           getInstance() { return s_instance; }

    void init();

private:
    static MDNSService*           s_instance;
};

#endif