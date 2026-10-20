-- Test fixture only: add bounded RSM index addressing to Prosody's normal MUC
-- MAM handler. Storage, authorization and result generation remain Prosody's.
-- Stock Prosody 13 omits first@index and does not implement index retrieval.
local st = require "prosody.util.stanza";
local jid = require "prosody.util.jid";
local archive = module:open_store("muc_log", "archive");
local mam = "urn:xmpp:mam:2";
local rsm = "http://jabber.org/protocol/rsm";
module:hook("iq-set/bare/" .. mam .. ":query", function(event)
    local query = event.stanza:get_child("query", mam);
    local set = query and query:get_child("set", rsm);
    local index_tag = set and set:get_child("index");
    if not index_tag then return; end
    local index = tonumber(index_tag:get_text());
    if not index or index < 0 then return; end
    local before;
    if index > 0 then
        local data = archive:find(jid.node(event.stanza.attr.to), {
            with = "message<groupchat"; limit = index; reverse = false;
        });
        if not data then return; end
        local at = 0;
        for uid in data do
            at = at + 1;
            if at == index then before = uid; break; end
        end
        if not before then return; end
    end
    set:remove_children("index");
    set:remove_children("before");
    set:remove_children("after");
    if before then set:text_tag("after", before); end
    local origin, iq_id = event.origin, event.stanza.attr.id;
    local original_send = origin.send;
    origin.send = function(reply)
        if type(reply) == "table" and reply.name == "iq" and reply.attr.id == iq_id then
            local fin = reply:get_child("fin", mam);
            local result_set = fin and fin:get_child("set", rsm);
            local first = result_set and result_set:get_child("first");
            if first then first.attr.index = tostring(index); end
            origin.send = original_send;
        end
        return original_send(reply);
    end;
    -- Continue into the ordinary handler, using a real opaque cursor.
end, 100);
