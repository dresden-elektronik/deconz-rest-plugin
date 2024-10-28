/*
 * Handle Hue-specific Dynamic Scenes.
 */

#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

/*! Scenes REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleHueScenesApi(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);
    if (rsp.map.isEmpty())
    {
        rsp.str = "{}"; // return empty object
    }
    rsp.httpStatus = HttpStatusOk;
    return REQ_READY_SEND;
}
