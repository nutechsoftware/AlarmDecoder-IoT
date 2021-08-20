var panel_states =  {
  "unknown":{
    "status_class": "not_ready",
    "icon_class": "icon house with_key",
    "label":"No partition state found. vpartID not found on AD2IoT check vpart config settings.",
    "b1_label": "Unknown",
    "b2_label": "Unknown"
  },
  "armed_away":{
    "status_class":"armed",
    "icon_class":"icon house with_key",
    "label":"Armed (away)",
    "b1_icon_class": "icon house with_lock",
    "b1_label": "DISARM",
    "b2_icon_class": "icon house with_walker",
    "b2_label": "DISARM"
  },
  "armed_away_exit":{
    "status_class":"armed",
    "icon_class":"icon house with_walker",
    "label":"Armed (away&nbsp;exit&nbsp;now)",
    "b1_icon_class": "icon house with_lock",
    "b1_label": "DISARM",
    "b2_icon_class": "icon house with_lock",
    "b2_label": "DISARM"
  },
  "armed_stay":{
    "status_class":"armed",
    "icon_class":"icon house with_key",
    "label":"Armed (stay)",
    "b1_icon_class": "icon house with_lock",
    "b1_label": "DISARM",
    "b2_icon_class": "icon house with_walker",
    "b2_label": "EXIT"
  },
  "armed_stay_exit":{
    "status_class":"armed",
    "icon_class":"icon house with_walker",
    "label":"Armed (stay&nbsp;exit&nbsp;now)",
    "b1_icon_class": "icon house with_lock",
    "b1_label": "DISARM",
    "b2_icon_class": "icon house with_lock",
    "b2_label": "DISARM"
  },
  "armed_night":{
    "status_class":"armed",
    "icon_class":"icon house with_key",
    "label":"Armed (night)",
    "b1_icon_class": "icon house with_lock",
    "b1_label": "DISARM",
    "b2_icon_class": "icon house with_walker",
    "b2_label": "EXIT"
  },
  "armed_night_exit":{
    "status_class":"armed",
    "icon_class":"icon house with_walker",
    "label":"Armed (exit&nbsp;now)",
    "b1_icon_class": "icon house with_lock",
    "b1_label": "DISARM",
    "b2_icon_class": "icon house with_walker",
    "b2_label": "EXIT"
  },
  "alarming":{
    "status_class":"alarming",
    "icon_class":"icon house with_key",
    "label":"Alarming!",
    "b1_icon_class": "icon house with_lock",
    "b1_label": "DISARM",
    "b2_icon_class": "icon house with_lock",
    "b2_label": "DISARM"
  },
  "fire":{
    "status_class":"alarming",
    "icon_class":"icon house with_key",
    "label":"Fire Alarm",
    "b1_icon_class": "icon house with_lock",
    "b1_label": "DISARM",
    "b2_icon_class": "icon house with_lock",
    "b2_label": "DISARM"
  },
  "ready":{
    "status_class":"ready",
    "icon_class":"icon house with_lock",
    "label":"Ready",
    "b1_icon_class": "icon house with_key",
    "b1_label": "AWAY",
    "b2_icon_class": "icon house with_key",
    "b2_label": "STAY"
  },
  "notready":{
    "status_class":"not_ready",
    "icon_class":"icon house with_lock",
    "label":"Not Ready",
    "b1_icon_class": "icon house with_key",
    "b1_label": "DISARM",
    "b2_icon_class": "icon house with_key",
    "b2_label": "DISARM"
  }
}
panel_states.get = function(key) {
  return (panel_states[key] ? panel_states[key] : panel_states["unknown"]);
}

var Debugger = function(klass) {
  this.debug = {}
  if (klass.isDebug) {
    for (var m in console)
      if (typeof console[m] == "function")
        this.debug[m] = console[m].bind(window.console, klass.toString()+": ");
  }else{
    for (var m in console)
      if (typeof console[m] == "function")
        this.debug[m] = function(){}
  }
  return this.debug;
}

const elem = id => document.getElementById(id);

class AD2ws {
  constructor(vpartID, codeID, wsHost) {
      this.isDebug = true;
      this.debug = Debugger(this);
      this.vpartID = vpartID;
      this.codeID = codeID;
      this.wsHost = wsHost;
      this.connecting = false;
      this.connected = false;
      this.ws = null;
      this.ad2emb_state = null;
      this.mode = "unknown";
  }

  /* init UI */
  initUI() {

    const divStatus = elem("divStatus");

    // alarm button click handler.
    function alarmClick(obj, clicks, command) {
      switch(clicks) {
        case 1:
          obj.classList.add("tap_1"); obj.classList.remove("tap_2");
          break;
        case 2:
          obj.classList.remove("tap_1"); obj.classList.add("tap_2");
          break;
        case 3:
          ad2ws.sendCommand(command);
        default:
          auxHrefClicks = 0;
          obj.classList.remove("tap_1"); obj.classList.remove("tap_2");
      }
    }

    /**
     * bind onclick events to the UI buttons
     */
    elem("b1Href").onclick = function() {
      ad2ws.sendCommand(elem("b1_text").innerHTML);
    };

    elem("b2Href").onclick = function() {
      ad2ws.sendCommand(elem("b2_text").innerHTML);
    };

    var panicHrefClicks = 0;
    var panicHrefTimer;
    elem("panicHref").onclick = function() {
      var obj = this;
      panicHrefClicks++;
      alarmClick(this, panicHrefClicks, "PANIC_ALARM");
      clearTimeout(panicHrefTimer);
      panicHrefTimer = setTimeout(function(){ panicHrefClicks=0; alarmClick(obj, panicHrefClicks, '');}, 3000);
    };

    var fireHrefClicks = 0;
    var fireHrefTimer;
    elem("fireHref").onclick = function() {
      var obj = this;
      fireHrefClicks++;
      alarmClick(this, fireHrefClicks, "FIRE_ALARM");
      clearTimeout(fireHrefTimer);
      fireHrefTimer = setTimeout(function(){ fireHrefClicks=0; alarmClick(obj, fireHrefClicks, "");}, 3000);
    };

    var auxHrefClicks = 0;
    var auxHrefTimer;
    elem("auxHref").onclick = function() {
      var obj = this;
      auxHrefClicks++;
      alarmClick(this, auxHrefClicks, "AUX_ALARM");
      clearTimeout(auxHrefTimer);
      auxHrefTimer = setTimeout(function(){ auxHrefClicks=0; alarmClick(obj, auxHrefClicks, "");}, 3000);
    };

    elem("refreshHref").onclick = function() {
      ad2ws.wsSendSync();
    };
  }

  /* update UI from current state */
  updateUI() {
    /* test if state is valid */
    if (!this.ad2emb_state) {
      this.mode = "unknown";
      return;
    }

    /* simple state machine for now to set status class */
    if (this.ad2emb_state.alarm_sounding) {
      this.mode = "alarming";
    } else
    if (this.ad2emb_state.fire_alarm) {
      this.mode = "fire";
    } else
    if (this.ad2emb_state.ready) {
      this.mode = "ready";
    } else {
      if (this.ad2emb_state.armed_away) {
        /* Note: append _exit if exit_now state is set */
        this.mode = "armed_away" + (this.ad2emb_state.exit_now ? "_exit" : "");
      } else
      if (this.ad2emb_state.armed_stay) {
        /* Note: append _exit if exit_now state is set */
        this.mode = "armed_stay" + (this.ad2emb_state.exit_now ? "_exit" : "");
      } else {
        if (this.ad2emb_state.last_alpha_message === "Unknown") {
          this.mode = "unknown";
        } else {
          this.mode = "notready";
        }
      }
    }

    // Update panel attributes if the mode changed.
    if (this.mode != this.last_mode) {
      this.last_mode = this.mode;

      /* retrieve the panel state for this mode and apply to the UI main panel. */
      debug.info("Setting the UI mode to '" + this.mode +"'");
      var ps = panel_states.get(this.mode);
      elem("status").className = ps.status_class;
      elem("status_icon").className = ps.icon_class;
      elem("status_text").innerHTML = ps.label;
      elem("b1_icon").className = ps.b1_icon_class;
      elem("b1_text").innerHTML = ps.b1_label;
      elem("b2_icon").className = ps.b2_icon_class;
      elem("b2_text").innerHTML = ps.b2_label;
    }
  }

  /* FIXME: docs on simple ws request api */
  /* connect WS to AD2 IoT device and stay connected. */
  wsConnect() {
      if (this.ws === null) {
        var wshost = document.location.host;
        if (wshost === "") {
          wshost = this.wsHost;
        }
        var url = "ws://" + wshost + "/ad2ws";
        this.debug.info("Connecting to ws url: "+url);
        divStatus.innerHTML = "<p>Connecting.</p>";
        this.connecting = true;
        this.ws = new WebSocket(url);

        /* On open send SYNC request for the virtual partition id */
        this.ws.onopen = e => {
            this.debug.info("Web socket open");
            this.connecting = false;
            this.connected = true;
            divStatus.innerHTML = "<p>Connected.</p>";
            this.wsSendSync();
        };
        /* Parse web socket message from the AD2Iot device */
        this.ws.onmessage = e => {
            if (e.data[0] == "{") {
              this.debug.info("Received AD2IoT alarm state: " + e.data);
              this.ad2emb_state = JSON.parse(e.data);
            }
            if (e.data[0] == "!") {
              if (e.data.startsWith("!PONG:")) {
                this.debug.info("Received 'PONG' AD2IoT web socket is alive.");
              } else {
                this.debug.info("Received unknown message from AD2IoT web socket: " + e.data);
              }
            }
            this.updateUI();
        }
        /* reconnect on lost connection. */
        this.ws.onclose = e => {
            this.debug.info("Web socket closed");
            this.ws = null;
            divStatus.innerHTML = "<p>Closed.</p>";
            if (this.connecting) {
              this.connecting = false;
              this.debug.info("was connecting.");
            }
            if (this.connected) {
                this.connected = false;
                divStatus.innerHTML+="<p>Disconnected.</p>";
            }
            setTimeout(function() {
              ad2ws.wsConnect();
            }, 1000);
        }
        /* on errors */
        this.ws.onerror = e => {
            this.debug.error(e);
            this.ws.close();
        }
      }
  }

 /**
  * Request the current AD2EMB AlarmDecoder state and inform the server
  * of our address or partition to be connected to.
  */
  wsSendSync() {
    this.debug.info("wsSendSync");
    this.wsSendMessage("!SYNC:"+this.vpartID+","+this.codeID);
  }

  /* check the ws connection */
  wsCheck() {
    this.debug.info("wsCheck");
    if (this.ws !== null) {
      if (this.ws.readyState === WebSocket.OPEN) {
        this.debug.info("sending ping.");
        this.wsSendMessage("!PING:00000000");
      }
    }
  }

  /* send message to AD2EMB IoT web socket. */
  wsSendMessage(msg) {
      if (this.ws !== null) {
          this.ws.send(msg);
      }
  }

  /* process a command convert it into the correct syntax for ad2ws_handler() in the AD2IoT firmware webUI component.
      ['<EXIT>', '<STAY>', '<AWAY>', '<DISARM>', '<CHIME>', '<BYPASS>'] */
  sendCommand(cmd) {
    debug.info("sendCommand(" + cmd + ")");
    /* if we have something send it. */
    if (cmd.length) {
      ad2ws.wsSendMessage("!SEND:<"+cmd+">");
    }

  }
};

/* helper to get values from the query string on the url. */
function getQueryStringParameterByName(name, url = window.location.href) {
  name = name.replace(/[\[\]]/g, '\\$&');
  var regex = new RegExp('[?&]' + name + '(=([^&#]*)|&|#|$)'),
      results = regex.exec(url);
  if (!results) return null;
  if (!results[2]) return '';
  return decodeURIComponent(results[2].replace(/\+/g, ' '));
}

/**
 * Use commands 'vpart' and 'code' to configure the virtual keypad and code slot.
 *  https://github.com/nutechsoftware/AlarmDecoder-STSDK#base-commands
 */
var vpartID = 0; var codeID = 0;
var szvalue = null;
if( (szvalue = getQueryStringParameterByName("vpartID")) === null ) {
  console.info("No 'vpartID' value provided in URI using default value.");
} else {
  console.info("Loading 'vpartID' value from URI.");
  vpartID = parseInt(szvalue);
};
if( (szvalue = getQueryStringParameterByName("codeID")) === null ) {
  console.info("No 'codeID' id value provided in URI using default value.");
} else {
  console.info("Loading 'codeID' id value from URI.");
  codeID = parseInt(szvalue);
};

/**
 * Allow for debugging by use a different host for the websocket than the source of this html.
 */
var wsHost = null;
if( (szvalue = getQueryStringParameterByName("wsHost")) === null ) {
  console.info("No 'wsHost' value provided in URI using default source host.");
} else {
  console.info("Loading 'wsHost' value from URI.");
  wsHost = szvalue;
};

console.info("Starting the AD2IoT Virtual Keypad using vpart id: "+ vpartID + " and code id: " + codeID);
/* Initialize the AD2ws class for the address */
let ad2ws = new AD2ws(vpartID, codeID, wsHost);

/* Initialize the UI */
ad2ws.initUI();

/* Start the web socket connection. */
ad2ws.wsConnect();

/* Create ping keep alive timer. */
var loadingTimer = setInterval(function(){ad2ws.wsCheck();}, 15000);
