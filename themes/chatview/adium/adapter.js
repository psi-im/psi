try {

window[chatServer.jsNamespace()].adapter = {
	loadTheme : function() {
        chatServer.setCaseInsensitiveFS();
		var chat = window[chatServer.jsNamespace()];
		//chat.console("DEBUG: loading " + chatServer.themeId());
		var resources = ["Template.html", "FileTransferRequest.html",
		"Footer.html", "Header.html", "Status.html", "Topic.html", "Content.html",
		"Incoming/Content.html", "Incoming/NextContent.html",
		"Incoming/Context.html", "Incoming/NextContext.html",
		"Outgoing/Content.html", "Outgoing/NextContent.html",
		"Outgoing/Context.html", "Outgoing/NextContext.html"];
		for (var i=0; i<resources.length; i++) {
            var content = chatServer.getFileContents("Contents/Resources/" + resources[i]);
			if (content.length) {
				chatServer.toCache(resources[i], content);
			}
		}
		//chat.console("DEBUG: 2");
		var ipDoc = chat.util.loadXML("Contents/Info.plist");
		var xres = ipDoc.evaluate("/plist/dict/*", ipDoc, null, null, null);
		var e;
		var curKey = "";
		var ip = {variants: {}};
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
		//chat.console("DEBUG: 3");
		var baseHtml = chatServer.cache("Template.html");
		if (baseHtml) {
			chatServer.toCache("html", baseHtml);
			if (!ip.MessageViewVersion) { // is not set. trying to guess
				if (baseHtml.indexOf("replaceLastMessage") != -1) {
					ip.MessageViewVersion = 4;
				} else if (baseHtml.indexOf("appendMessageNoScroll") != -1) {
					ip.MessageViewVersion = 3;
				} else  {
					ip.MessageViewVersion = 2; // or less
				}
			}
		} else {
			chatServer.toCache("html", chatServer.getFileContentsFromAdapterDir("Template.html"));
			ip.MessageViewVersion = 4;
		}
		if (!String(chatServer.cache("html")).length) {
			return "Failed to cache Template.html";
		}
		chatServer.toCache("Info.plist", ip);
		chatServer.setMetaData({name: ip.CFBundleName});
		var jsRootName = "_adiumChat"+Math.round(1000*Math.random());
		window[jsRootName] = window[chatServer.jsNamespace()];
		chatServer.setJSNamespace(jsRootName);
		chatServer.setDefaultAvatar("Contents/Resources/Incoming/buddy_icon.png", "incoming");
		chatServer.setDefaultAvatar("Contents/Resources/Outgoing/buddy_icon.png", "outgoing");
		return "ok";
	}
}

// update apapter with methods having some private part
window[chatServer.jsNamespace()].util.updateObject(window[chatServer.jsNamespace()].adapter, function(){
	var chat = window[chatServer.jsNamespace()];
	var server = window.chatServer;
	var session = null;
	var dateFormat = "hh:mm";
	var cdata;
	var proxyEl = document.createElement("div");

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
			} else if (this.name == "userIconPath") {
				return "avatar://" + (cdata.local?"outgoing/":"incoming/") +
					encodeURIComponent(session.account()) +
					"/" + encodeURIComponent(cdata.userid);
			} else if (this.name == "incomingIconPath") {
				return "avatar://incoming/" + encodeURIComponent(session.account()) +
					"/" + encodeURIComponent(session.jid());
			} else if (this.name == "senderColor") {
				return session.mucNickColor(cdata.sender, cdata.local);
			}
			return d || "";
		}
	}

	function TemplateTimeVar(name, param) {
		this.name = name;
		if (param) {
			var i, r = {y:'yy',Y:'yyyy',m:'MM',d:'dd',H:'hh',M:'mm',S:'ss'};
			var m = param.split(/%([a-zA-Z]+)/)
			for (i=0; i<m.length; i++) {
				m[i] = r[m[i]] || m[i];
			}
			this.format = m.join("");
		} else {
			this.format = dateFormat
		}
	}

	TemplateTimeVar.prototype.toString = function() {
		//chat.console("DEBUG: TemplateTimeVar.prototype.toString");
		return cdata[this.name] instanceof Date?
			server.formatDate(cdata[this.name], this.format) :
			(cdata[this.name] ? cdata[this.name] : "");
	}

	function Template(raw) {
		var splitted = raw.split(/(%[\w]+(?:\{[\w:%]+\})?%)/), i;
		this.parts = [];

		for (i = 0; i < splitted.length; i++) {
			var m = splitted[i].match(/%([\w]+)(?:\{([\w:%]+)\})?%/);
			if (m) {
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
		getHtml : function() {
			session = window.chatSession; // global session because Template needs it

			//chat.console("prepare html");
			var html = server.cache("html");
			//chat.console("cached Template.html: " + html);
			var ip = server.cache("Info.plist");
			var variant = ip.DefaultVariant;
			if (variant && ip.variants[variant]) {
				chat.util.updateObject(ip, ip.variants[variant]);
			}
			////chat.console("prepare html2");
			var topHtml = (session.isMuc() && server.cache("Topic.html")) || server.cache("Header.html");
			//chat.console("prepare html2.5");
			topHtml = topHtml? (new Template(topHtml)).toString({
				chatName: chat.util.escapeHtml(session.chatName()),
				timeOpened: new Date()
			}) : "";
			//chat.console("prepare html3");
			var footerHtml = new Template(server.cache("Footer.html") || "").toString({});
			footerHtml += "\n<script type='text/javascript'>window.addEventListener('load', "+
				server.jsNamespace()+".adapter.initSession, false);</script>";
			//chat.console("prepare html4");
			//var baseUrl = "/" + server.themeId() + "/Contents/Resources//"; // double slash is hack to make relative css import path working
			var baseUrl = window.location.href + "Contents/Resources//";
			if (ip.MessageViewVersion < 3) {
				var replace = [
					baseUrl, "main.css",
					topHtml, footerHtml
				];
			} else {
				var replace = [
					baseUrl, "@import url( \"main.css\" );",
					ip.DefaultVariant? "Variants/" + ip.DefaultVariant + ".css" : "main.css",
					topHtml, footerHtml
				];
			}
			//chat.console("prepare html5");
			html = html.replace(/%@/g, function(){return replace.shift() || ""});
			if (ip.DefaultBackgroundIsTransparent) {
				session.setTransparent();
			} else {
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
			return html
		},
		initSession : function() {
			//chat.console("init session");
			session = window.chatSession;
			chat.adapter.initSession = null;
			chat.adapter.loadTheme = null;
			chat.adapter.getHtml = null;
			var trackbar = null;
			var ip = server.cache("Info.plist");
			var prevGrouppingData = null;
			var groupping = !(ip.DisableCombineConsecutive == true);

			chat.adapter.receiveObject = function(data) {
				cdata = data;
				try {
					//chat.console(chat.util.props(data, true))
					var template;
					if (data.type == "message") {
						if (data.mtype != "message") {
							prevGrouppingData = null;
						}
						switch (data.mtype) {
							case "message":
								data.nextOfGroup = groupping && !!(prevGrouppingData &&
									(prevGrouppingData.type == cdata.type) &&
									(prevGrouppingData.mtype == cdata.mtype) &&
									(prevGrouppingData.userid == cdata.userid) &&
									(prevGrouppingData.emote == cdata.emote) &&
									(prevGrouppingData.local == cdata.local));

								if (data.nextOfGroup) {
									template = data.local?templates.outgoingNextContent:templates.incomingNextContent;
								} else {
									template = data.local?templates.outgoingContent:templates.incomingContent;
								}
								prevGrouppingData = data;
								data.senderStatusIcon="icon:status/online"; //FIXME temporary hack
								break;
							case "status":
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
								break;
							case "subject": //its better to init with proper templates on start than do comparision like below
								template = templates.status;
								var e = document.getElementById("topic");
								if (e) {
									e.innerHTML = data["usertext"]
								} else {
									data["message"] += ("<br/>" + data["usertext"]);
								}
								break;
							case "urls":
								var i, urls=[];
								for (url in data.urls) {
									urls.push('<a href="'+url+'">'+(data.urls[url]?chat.util.escapeHtml(data.urls[url]):url)+"</a>");
								}
								data["message"] = urls.join("<br/>");
								template = templates.status;
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
					} else if (data.type == "clear") {
						prevGrouppingData = null; //groupping impossible
						trackbar = null;
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
				var content = server.cache(tcList[i]);
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
			chat.console("session inited");
			session.signalInited();
		}
	}
}())

} catch(e) {
	window[chatServer.jsNamespace()].console("adapter load error!!! "+ e + "(Line:" + e.line + ")");
}
"ok"; // just an indicator for script loader
