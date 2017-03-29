function psiThemeAdapter(chat)
{

var adapter = {
	loadTheme : function() {
		//var chat = chat;
        var loader = window.srvLoader;
        chat.console("DEBUG: loading " );
        loader.setCaseInsensitiveFS(true);
        loader.setPrepareSessionHtml(true);
        loader.setHttpResourcePath("/Contents/Resources");
		chat.console("DEBUG: loading " + loader.themeId);
		var resources = ["FileTransferRequest.html",
		"Footer.html", "Header.html", "Status.html", "Topic.html", "Content.html",
		"Incoming/Content.html", "Incoming/NextContent.html",
		"Incoming/Context.html", "Incoming/NextContext.html",
		"Outgoing/Content.html", "Outgoing/NextContent.html",
		"Outgoing/Context.html", "Outgoing/NextContext.html"];

        var toCache = {};
        for (var i=0; i<resources.length; i++) {
            toCache[resources[i]] = "Contents/Resources/" + resources[i];
        }
        loader.saveFilesToCache(toCache);

        function plistLoaded(ipDoc)
        {
            var xres = ipDoc.evaluate("/plist/dict/*", ipDoc, null, null, null);
            var e;
            var curKey = "";

            var varKey = null;
            var value;
            while (e = xres.iterateNext()) {
                //chat.console("DEBUG: " + e.textContent);
                switch(e.tagName) {
                    case "key":
                        curKey = e.textContent.split(':');
                        varKey = null;
                        if (curKey.length == 2) {
                            varKey = curKey[1];
                        }
                        curKey = curKey[0];
                        continue;
                    case "string":
                        value = e.textContent;
                        break;
                    case "integer":
                        value = Number(e.textContent);
                        break;
                    case "false":
                        value = false;
                        break;
                    case "true":
                        value = true;
                        break;
                    default:
                        value = undefined;
                }
                //chat.console("DEBUG: 2.5");
                if (varKey) {
                    if (!ip.variants[varKey]) {
                        ip.variants[varKey] = {};
                    }
                    ip.variants[varKey][curKey] = value;
                } else {
                    ip[curKey] = value;
                }
            }
            loader.setMetaData({name: ip.CFBundleName});
            if (ip.DefaultBackgroundIsTransparent) {
                loader.setTransparent();
            }

            var resources = ["Incoming/buddy_icon.png", "Outgoing/buddy_icon.png", "incoming_icon.png", "outgoing_icon.png"];
            loader.checkFilesExist(resourcesListReady, "Contents/Resources");
        }

        function resourcesListReady(rlist)
        {
            var avatars = {}
            avatars.incomingBuddy = rlist["Incoming/buddy_icon.png"]? "Incoming/buddy_icon.png" : chat.defaultAvatarUrl;
            avatars.outgoingBuddy = rlist["Outgoing/buddy_icon.png"]? "Outgoing/buddy_icon.png" : chat.defaultAvatarUrl;
            avatars.incomingImage = rlist["incoming_icon.png"]? "incoming_icon.png" : chat.defaultAvatarUrl;
            avatars.outgoingImage = rlist["outgoing_icon.png"]? "outgoing_icon.png" : chat.defaultAvatarUrl;
            loader.toCache("avatars", avatars)
            loader.getFileContents("Contents/Resources/Template.html", baseHtmlLoaded);
        }

        function baseHtmlLoaded(baseHtml)
        {
            if (baseHtml) {
                if (!ip.MessageViewVersion) { // is not set. trying to guess
                    if (baseHtml.indexOf("replaceLastMessage") != -1) {
                        ip.MessageViewVersion = 4;
                    } else if (baseHtml.indexOf("appendMessageNoScroll") != -1) {
                        ip.MessageViewVersion = 3;
                    } else  {
                        ip.MessageViewVersion = 2; // or less
                    }
                }
                baseHtmlRecognized(baseHtml);
            } else {
                ip.MessageViewVersion = 4;
                loader.getFileContentsFromAdapterDir("Template.html", baseHtmlRecognized);
                //loader.toCache("html", loader.getFileContentsFromAdapterDir("Template.html"));
            }
        }

        function baseHtmlRecognized(baseHtml)
        {
            if (!baseHtml.length) {
                loader.errorThemeLoading("Failed to retrieve Template.html");
                return
            }
            loader.toCache("html", baseHtml);
            loader.toCache("Info.plist", ip);
            chat.server.jsNamespace = "_adiumChat"+Math.round(1000*Math.random());
            window[chat.server.jsNamespace] = chat; // just to keep it the same and avoid any problems

            loader.finishThemeLoading();
        }

        var ip = {variants: {}};
		//chat.console("DEBUG: 2");
		chat.util.loadXML("Contents/Info.plist", plistLoaded);
	}
}

// update apapter with methods having some private part
chat.util.updateObject(adapter, function(chat){
    var server = chat.server;
    var loader = window.srvLoader;
	var session = null;
	var dateFormat = "%H:%M";
	var cdata;
	var proxyEl = document.createElement("div");
    var defaultAvatars = null;
    var avatarsMap = {};

	function TemplateVar(name, param) {
		this.name = name;
		this.param = param
	}

	TemplateVar.prototype = {
		toString : function() {
			//chat.console("DEBUG: TemplateVar.prototype.toString " + session);
			var d = cdata[this.name];
			if (this.name == "sender") { //may not be html
				d = chat.util.escapeHtml(d);
			} else if (d instanceof Date) {
				d = server.formatDate(d, "yyyy-MM-dd");
			} else if (this.name == "userIconPath") { // associated with message
                var url;
                if (cdata.local) {
                    url = session.localUserAvatar? session.localUserAvatar : defaultAvatars.outgoingBuddy;
                } else {
                    if (session.isMuc) {
                        url = avatarsMap[cdata.sender];
                    }
                    if (!url) {
                        url = session.remoteUserAvatar? session.remoteUserAvatar : defaultAvatars.incomingBuddy;
                    }
                }
                //chat.console((cdata.local? "local " : "remote ") + "avatar: " + url)
                return url;

			} else if (this.name == "incomingIconPath") { // associated with chat
                return session.remoteUserImage? session.remoteUserImage : defaultAvatars.incomingImage;
            } else if (this.name == "outgoingIconPath") { // associated with chat
                return session.localUserImage? session.localUserImage : defaultAvatars.ougoingImage;

			} else if (this.name == "senderColor") {
				return session.mucNickColor(cdata.sender, cdata.local);
			} else if (this.name == "message" && cdata.id) {
                // if we have an id then this is a replacable message.
                // next weird logic is as follows:
                //   - wrapping to some element may break elements flow
                //   - using well know elements may break styles
                //   - setting just starting mark is useless (we will never find correct end)
                return "<psims mid=\"" + cdata.id + "\"/>" + d + "<psime mid=\"" + cdata.id + "\"/>";
            }

			return d || "";
		}
	}

	function TemplateTimeVar(name, param) {
		this.name = name;
		this.param = param || dateFormat;
	}

	TemplateTimeVar.prototype.toString = function() {
		return cdata[this.name] instanceof Date?
			server.strftime(cdata[this.name], this.param) :
			server.strftime(new Date(), this.param);
	}

	function Template(raw) {
		//chat.console("parsing '"+raw+"'");
		var splitted = raw.split(/(%[\w]+(?:\{[^\{]+\})?%)/), i;
		this.parts = [];

		for (i = 0; i < splitted.length; i++) {
			var m = splitted[i].match(/%([\w]+)(?:\{([^\{]+)\})?%/);
			if (m) {
				//chat.console("found var '"+m[1]+"'");
				this.parts.push(m[1] in tvConstructors
					? new tvConstructors[m[1]](m[1], m[2])
					: new TemplateVar(m[1], m[2]));
			} else {
				this.parts.push(splitted[i]);
			}
		}
	}

	Template.prototype.toString = function(data) {
		//chat.console("prepare Template.prototype.toString1");
		cdata = data || cdata;
		var html = this.parts.join("");
		proxyEl.innerHTML = html;
		chat.util.replaceIcons(proxyEl);
		//chat.console("prepare Template.prototype.toString2");
		return proxyEl.innerHTML;
	}

	// Template variable constructors
	var tvConstructors = {
		time : TemplateTimeVar,
		timeOpened : TemplateTimeVar
	}

	function psiOption(name) {
		return eval("[" + server.psiOption(name) + "][0]")
	}

	return {
		generateSessionHtml : function(sessionId, serverSession, basePath) {
            session = serverSession;

            function onServerStuffReady(cache, sessProps) {
                defaultAvatars = cache.avatars
                //chat.console("prepare html");
                var html = cache["html"];
                //chat.console("cached Template.html: " + html);
                var ip = cache["Info.plist"];

                var variant = ip.DefaultVariant;
                if (variant && ip.variants[variant]) {
                    chat.util.updateObject(ip, ip.variants[variant]);
                }
                chat.console("prepare html2");
                var topHtml = (loader.isMuc && cache["Topic.html"]) || cache["Header.html"];
                chat.console("prepare html2.5");
                topHtml = topHtml? (new Template(topHtml)).toString({
                    chatName: chat.util.escapeHtml(sessProps.chatName),
                    timeOpened: new Date()
                }) : "";
                chat.console("prepare html3");
                var footerHtml = new Template(cache["Footer.html"] || "").toString({});

                footerHtml = "<script src=\"/psiglobal/qwebchannel.js\"></script> \
                        <script type=\"text/javascript\"> \
                            new QWebChannel(qt.webChannelTransport, function (channel) { \
                                window.srvSession = channel.objects.srvSession; \
                                window.srvUtil = channel.objects.srvUtil; \
                                initPsiTheme().adapter.initSession(); \
                            }); \
                    </script>" + footerHtml;

                //footerHtml += "\n<script type='text/javascript'>window.addEventListener('load', "+
                //    server.jsNamespace+".adapter.initSession, false);</script>";
                //chat.console("prepare html4");
                var baseUrl = loader.serverUrl + basePath + "/";
                var replace
                if (ip.MessageViewVersion < 3) {
                    replace = [
                        baseUrl, "main.css",
                        topHtml, footerHtml
                    ];
                } else {
                    replace = [
                        baseUrl, "@import url( \"main.css\" );",
                        ip.DefaultVariant? "Variants/" + ip.DefaultVariant + ".css" : "main.css",
                        topHtml, footerHtml
                    ];
                }
                chat.console("prepare html5");
                html = html.replace(/%@/g, function(){return replace.shift() || ""});
                chat.console("DEBUG: " + html);
                if (!ip.DefaultBackgroundIsTransparent) {
                    if (ip.DefaultBackgroundColor)
                        html = html.replace(/<head>/i, '<head><style type="text/css" media="screen,print">' +
                            "body { background-color:#"+ip.DefaultBackgroundColor+" }</style>");
                }

                var styles = [];
                if (ip.DefaultFontFamily) {
                    styles.push("font-family:"+ip.DefaultFontFamily);
                }
                if (ip.DefaultFontSize) {
                    styles.push("font-size:"+ip.DefaultFontSize+"pt");
                }

                html = html.replace("==bodyBackground==", styles.join(";"));
                //chat.console("prepare html: " + html);
                loader.setSessionHtml(sessionId, html);
            }

            server.loadFromCacheMulti(["html", "Info.plist", "Topic.html", "Header.html", "Footer.html", "avatars"],
                function (cache) {
                    loader.sessionProperties(sessionId, ["chatName"], function(props) {
                        onServerStuffReady(cache, props)
                    });
                });


		},
		initSession : function() {
			chat.console("init session");

            function cacheReady(cache)
            {
                session = window.srvSession;
                defaultAvatars = cache.avatars
                chat.console(chat.util.props(defaultAvatars, true))
                chat.adapter.initSession = null;
                chat.adapter.loadTheme = null;
                chat.adapter.getHtml = null;
                var trackbar = null;
                var ip = cache["Info.plist"];
                var prevGrouppingData = null;
                var groupping = !(ip.DisableCombineConsecutive == true);

                chat.adapter.receiveObject = function(data) {
                    cdata = data;
                    try {
                        chat.console(chat.util.props(data, true))
                        var template;
                        if (data.type == "message") {
                            if (data.mtype != "message") {
                                prevGrouppingData = null;
                            }
                            data.messageClasses = cdata.local?"outgoing" : "incoming";
                            data.messageClasses += cdata.alert?" mention" : "";
                            data.messageClasses += cdata.spooled?" history" : "";
                            data.messageClasses += cdata.mtype == "system"?" event" : "";
                            // TODO consecutive, autoreply, focus, firstFocus, %status%
                            switch (data.mtype) {
                                case "message":
                                    data.messageClasses += " message";
                                    data.nextOfGroup = groupping && !!(prevGrouppingData &&
                                        (prevGrouppingData.type == cdata.type) &&
                                        (prevGrouppingData.mtype == cdata.mtype) &&
                                        (prevGrouppingData.userid == cdata.userid) &&
                                        (prevGrouppingData.emote == cdata.emote) &&
                                        (prevGrouppingData.local == cdata.local));
                                    data.messageClasses += data.nextOfGroup? " consecutive" : "";

                                    if (data.nextOfGroup) {
                                        template = data.local?templates.outgoingNextContent:templates.incomingNextContent;
                                    } else {
                                        template = data.local?templates.outgoingContent:templates.incomingContent;
                                    }
                                    prevGrouppingData = data;
                                    data.senderStatusIcon="icon:status/online"; //FIXME temporary hack
                                    break;
                                case "status":
                                    data.messageClasses += " status";
                                case "system":
                                    if (data["usertext"]) {
                                        data["message"] += " (" + data["usertext"] + ")"
                                    }
                                    template = templates.status;
                                    break;
                                case "lastDate":
                                    data["message"] = data["date"];
                                    data["time"] = "&nbsp; &nbsp; &nbsp; &nbsp; &nbsp;"; //fixes some themes =)
                                    template = templates.status;
                                    data.messageClasses += " event date_separator";
                                    break;
                                case "subject": //its better to init with proper templates on start than do comparision like below
                                    template = templates.status;
                                    var e = document.getElementById("topic");
                                    if (e) {
                                        e.innerHTML = data["usertext"]
                                    } else {
                                        data["message"] += ("<br/>" + data["usertext"]);
                                    }
                                    data.messageClasses += " event";
                                    break;
                                case "urls":
                                    var i, urls=[];
                                    for (url in data.urls) {
                                        urls.push('<a href="'+url+'">'+(data.urls[url]?chat.util.escapeHtml(data.urls[url]):url)+"</a>");
                                    }
                                    data["message"] = urls.join("<br/>");
                                    template = templates.status;
                                    data.messageClasses += " event";
                                    break;
                            }
                            if (template) {
                                if (data.nextOfGroup) {
                                    appendNextMessage(template.toString(data));
                                } else {
                                    appendMessage(template.toString(data));
                                }
                                if (data.mtype == "message" && data.local) {
                                    scrollToBottom();
                                }
                            } else {
                                throw "Template not found";
                            }
                        } else if (data.type == "replace") {
                            var doScroll = nearBottom();
                            var cel = document.getElementById("Chat");
                            var se =cel.querySelector("psims[mid='"+data.replaceId+"'");
                            var ee =cel.querySelector("psime[mid='"+data.replaceId+"'");
                            if (se && ee && se.parentNode === ee.parentNode) {
                                while (se.nextSibling !== ee) {
                                    se.parentNode.removeChild(se.nextSibling);
                                }
                                var node = createHTMLNode(data.message);
                                ee.parentNode.insertBefore(node, ee);
                            }
                        } else if (data.type == "clear") {
                            prevGrouppingData = null; //groupping impossible
                            trackbar = null;
                        } else if (data.type == "avatar") {
                            if (cdata.avatar) {
                                avatarsMap[cdata.sender] = cdata.avatar;
                            } else {
                                delete avatarsMap[cdata.sender];
                            }
                        }
                    } catch(e) {
                        chat.util.showCriticalError("APPEND ERROR: " + e + " \nline: " + e.line)
                    }
                };

                var t = {};
                var templates = {}
                var tcList = ["Status.html", "Content.html",
                    "Incoming/Content.html", "Incoming/NextContent.html",
                    "Incoming/Context.html", "Incoming/NextContext.html",
                    "Outgoing/Content.html", "Outgoing/NextContent.html",
                    "Outgoing/Context.html", "Outgoing/NextContext.html"];
                var i
                for (i=0; i<tcList.length; i++) {
                    var content = cache[tcList[i]];
                    if (content) {
                        t[tcList[i]] = new Template(content);
                    }
                }
                templates.content = t["Content.html"] || "%message%";
                templates.status = t["Status.html"] || t.message;
                templates.incomingContent = t["Incoming/Content.html"] || templates.content;
                templates.outgoingContent = t["Outgoing/Content.html"] || templates.incomingContent;
                templates.incomingNextContent = t["Incoming/NextContent.html"] || templates.incomingContent;
                templates.outgoingNextContent = t["Outgoing/NextContent.html"] || templates.outgoingContent;
                templates.incomingContext = t["Incoming/Context.html"] || templates.incomingContent;
                templates.outgoingContext = t["Outgoing/Context.html"] || templates.outgoingContent;
                templates.incomingNextContext = t["Incoming/NextContext.html"] || templates.incomingNextContent;
                templates.outgoingNextContext = t["Outgoing/NextContext.html"] || templates.outgoingNextContent;
                delete t
                //t.lastMsgDate = t.lastMsgDate || t.sys;
                //t.subject = t.subject || t.sys;
                //t.urls = t.urls || t.sys;
                //t.trackbar = t.trackbar || "<hr/>";


                function printAvatar(newAvatar) {
                    chat.console("New local avatar: " + newAvatar);
                }

                session.localUserAvatarChanged.connect(printAvatar);

                session.newMessage.connect(chat.adapter.receiveObject);
                chat.console("session inited");
                session.signalInited();
            }

            server.loadFromCacheMulti(["Info.plist", "avatars", "Status.html", "Content.html",
                                      "Incoming/Content.html", "Incoming/NextContent.html",
                                      "Incoming/Context.html", "Incoming/NextContext.html",
                                      "Outgoing/Content.html", "Outgoing/NextContent.html",
                                      "Outgoing/Context.html", "Outgoing/NextContext.html"], cacheReady)
		}
	}
}(chat))

    chat.console("Adium adapter is ready");
    return adapter;
}
