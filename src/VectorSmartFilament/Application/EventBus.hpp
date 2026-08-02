#pragma once

#include <functional>
#include <string>
#include <vector>

namespace VectorSmartFilament {

struct FilamentEvent
{
    std::string name;
    std::string spool_uuid;
};

class FilamentEventBus
{
public:
    using Handler = std::function<void(const FilamentEvent&)>;

    void subscribe(Handler handler) { m_handlers.push_back(handler); }

    void publish(const FilamentEvent& event) const
    {
        for (const Handler& handler : m_handlers)
            handler(event);
    }

private:
    std::vector<Handler> m_handlers;
};

} // namespace VectorSmartFilament
