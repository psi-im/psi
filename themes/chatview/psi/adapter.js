function psiThemeAdapter(chat) {
    chat.console("Psi adapter is ready");

    return {
	loadTheme : function() {
        srvLoader.getFileContents("index.html", function(html){
            srvLoader.setHtml(html);
            srvLoader.getFileContents("load.js", function(js){
                eval(js);
                srvLoader.finishThemeLoading();
            })
        })
	},
	initSession : function() {
		var chat = chat;
		var trackbar = null;
		var inited = false;
		var proxy = null;

		var shared = {
			templates : {},
			server : window.srvUtil,
			session : window.srvSession,
			isMuc : window.srvSession.isMuc(),
			accountId : window.srvSession.account,
			dateFormat : "hh:mm:ss",
			scroller : null,
			varHandlers : {},
			prevGrouppingData : null,
			groupping : false,
			chatElement : null,

			TemplateVar : function(name) {
				this.name = name;
			},

			Template : function(raw) {
				var splitted = raw.split('%'), i;
				this.parts = [];

				for (i = 0; i < splitted.length; i++) {
					if (/^[a-zA-Z]+$/.test(splitted[i])) {
						this.parts.push(new shared.TemplateVar(splitted[i]));
					} else if (this.parts.length>0 && typeof(this.parts[this.parts.length-1]) == "string") {
						this.parts[this.parts.length-1]+=('%'+splitted[i]);
					} else {
						this.parts.push(splitted[i]);
					}
				}
			},

			psiOption : function(name) {
				return eval("[" + shared.server.psiOption(name) + "][0]")
			},

			colorOption : function(name) {
				return eval("[" + shared.server.colorOption(name) + "][0]")
			},

			appendHtml : function(html, scroll, nextEl) { //scroll[true|false|auto/other]
				if (typeof(scroll) == 'boolean') {
					shared.scroller.atBottom = scroll;
				}
				if (nextEl) {
					chat.util.siblingHtml(nextEl, html);
				} else {
					chat.util.appendHtml(shared.chatElement, html);
				}
				shared.scroller.invalidate();
			},

			stopGroupping : function() {
				if (shared.prevGrouppingData) {
					shared.prevGrouppingData.nextEl.parentNode.removeChild(shared.prevGrouppingData.nextEl)
					shared.prevGrouppingData = null;
				}
			},

			initTheme : function(config) {
				if (inited) {
					chat.util.showCriticalError("Theme should not be inited twice. Something wrong with theme.")
					return false;
				}
				var t = shared.templates;
				shared.chatElement = config.chatElement;
				shared.dateFormat = config.dateFormat || shared.dateFormat;
				shared.scroller = config.scroller || new chat.WindowScroller(false);
				shared.groupping = config.groupping || shared.groupping;
				proxy = config.proxy;
				shared.varHandlers = config.varHandlers || {};
				for (var tname in config.templates) {
					if (config.templates[tname]) {
						t[tname] = new shared.Template(config.templates[tname]);
					}
				}
				t.message = t.message || "%message%";
				t.sys = t.sys || "%message%";
				t.sysMessage = t.sysMessage || t.sys;
				t.sysMessageUT = t.sysMessageUT || t.sysMessage;
                t.statusMessageUT = t.statusMessageUT || (t.statusMessage || t.sysMessageUT);
				t.statusMessage = t.statusMessage || t.sysMessage;
				t.sentMessage = t.sentMessage || t.message;
				t.receivedMessage = t.receivedMessage || t.message;
				t.spooledMessage = t.spooledMessage || t.message;
				t.receivedMessageGroupping = t.receivedMessageGroupping || t.messageGroupping;
				t.sentMessageGroupping = t.sentMessageGroupping || t.messageGroupping;
				t.lastMsgDate = t.lastMsgDate || t.sys;
				t.subject = t.subject || t.sys;
				t.urls = t.urls || t.sys;
				t.trackbar = t.trackbar || "<hr/>";
				//config.defaultAvatar && shared.server.setDefaultAvatar(config.defaultAvatar)
				//config.avatarSize && shared.server.setAvatarSize(config.avatarSize.width, config.avatarSize.height);
				inited = true;
			},
			checkNextOfGroup : function() {
				shared.cdata.nextOfGroup = !!(shared.prevGrouppingData &&
									(shared.prevGrouppingData.type == shared.cdata.type) &&
									(shared.prevGrouppingData.mtype == shared.cdata.mtype) &&
									(shared.prevGrouppingData.userid == shared.cdata.userid) &&
									(shared.prevGrouppingData.emote == shared.cdata.emote) &&
									(shared.prevGrouppingData.local == shared.cdata.local));
				return shared.cdata.nextOfGroup;
			}
		};

        // internationalization
        function tr(text)
        {
            // TODO translate
            return text;
        }

        // accepts some templater object and object with 2 optional methods: "pre" and "post(text)"
        function proxyTemplate(template, handlers)
        {
            return {"toString": function(){
                if(handlers.pre) handlers.pre();
                var result = template.toString();
                if(handlers.post) result = handlers.post(result);
                return result;
            }}
        }

		shared.TemplateVar.prototype = {
			toString : function() {
				if (shared.varHandlers[this.name]) {
					return shared.varHandlers[this.name]();
				}
				var d = shared.cdata[this.name];
				if (this.name == "sender") { //may not be html
					d = chat.util.escapeHtml(d);
				} else if (d instanceof Date) {
					if (this.name == "time") {
						d = shared.server.formatDate(d, shared.dateFormat);
					} else { // last message date ?
						d = shared.server.formatDate(d, "yyyy-MM-dd");
					}
				 } else if (this.name == "avatarurl") {
					return "avatar:" + encodeURIComponent(shared.accountId) +
						"/" + encodeURIComponent(shared.cdata.userid);
				} else if (this.name == "next") {
					shared.cdata.nextEl = "nextMessagePH"+(1000+Math.floor(Math.random()*1000));
					return '<div id="'+shared.cdata.nextEl +'"></div>';
				}
				return d || "";
			}
		}

		shared.Template.prototype.toString = function() {
			return this.parts.join("");
		}


		chat.adapter.receiveObject = function(data) {
			shared.cdata = data;
			if (!inited) {
				chat.util.showCriticalError("A try to output data while theme is not inited. output is impossible.\nCheck if your theme does not have errors.");
				return;
			}
			try {
				//shared.server.console(chat.util.props(data, true))
				var template;
				if (proxy && (template = proxy()) === false) { // proxy stopped processing
					return; //we don't store shared.prevGrouppingData here, let's proxy do it if needed
				}
				if (data.type == "message") {
					if (data.mtype != "message") {
						shared.stopGroupping();
					}
					if (!template) switch (data.mtype) {
						case "message":
							if (shared.checkNextOfGroup()) {
								template = data.local?shared.templates.sentMessageGroupping:shared.templates.receivedMessageGroupping;
							}
							if (!template) {
								data.nextOfGroup = false; //can't group w/o template
								template = data.local?shared.templates.sentMessage:shared.templates.receivedMessage;
							}
							break;
                        case "status":
                            template = data.usertext?shared.templates.statusMessageUT:shared.templates.statusMessage;
							break;
						case "system":
							template = data.usertext?shared.templates.sysMessageUT:shared.templates.sysMessage;
							break;
						case "lastDate":
							template = shared.templates.lastMsgDate;
							break;
						case "subject": //its better to init with proper templates on start than do comparision like below
							template = shared.templates.subject;
							break;
						case "urls":
							var i, urls=[];
							for (url in data.urls) {
								urls.push('<a href="'+url+'">'+(data.urls[url]?chat.util.escapeHtml(data.urls[url]):url)+"</a>");
							}
							data["message"] = urls.join("<br/>");
							template = shared.templates.urls;
							break;
					}
					if (template) {
						shared.appendHtml(template.toString(), data.local?true:null, data.nextOfGroup?
							shared.prevGrouppingData.nextEl:null); //force scroll on local messages
						shared.stopGroupping();// safe clean up previous data
						if (shared.cdata.nextEl) { //convert to DOM
							shared.cdata.nextEl = document.getElementById(shared.cdata.nextEl);
							shared.prevGrouppingData = shared.cdata;
						}
					} else {
						throw "Template not found";
					}
				} else if (data.type == "trackbar") {
					if (!trackbar) {
						trackbar = document.createElement("div");
						trackbar.innerHTML = shared.templates.trackbar.toString();
					} else {
						shared.chatElement.removeChild(trackbar);
					}
					shared.chatElement.appendChild(trackbar);
					shared.scroller.invalidate();
					shared.stopGroupping(); //groupping impossible
				} else if (data.type == "clear") {
					shared.stopGroupping(); //groupping impossible
					shared.chatElement.innerHTML = "";
					trackbar = null;
				}
			} catch(e) {
				chat.util.showCriticalError("APPEND ERROR: " + e + " \nline: " + e.line)
			}
		};

		chat.adapter.initSession = null;
		chat.adapter.loadTheme = null;
        //window.srvLoader = null;
		//window.chatServer = null;
		window.chatSession = null;
		return shared;

	}
};

}
