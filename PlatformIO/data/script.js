/***********************************************************************
 *   BME:4920 - Team 13 | Remotely Controlled Hand Exoskeleton
 *          Sullivan Bryant, Charley Dunham, Jared Gilliam
 *                  ----------------------
 *
 *   ====================== script.js ======================
 *   This document manages the JavaScript on the index.html page,
 *      allowing for real-time event-driven updates. This document
 *      manages the websocket connection to stream device data
 *      and control the device.
 ***********************************************************************/


/**
 * This method is called upon the page loading, instantiating three
 *  custom classes defined in this document, as well as instantiating
 *  a Smoothie
 */
document.addEventListener("DOMContentLoaded", () => {
    /* Connect to a new WebSocket hosted on the same server on the ws endpoint. */
    const ws = new WSClient(`ws://${location.host}/ws`);
    /* Instantiate a graph object */
    const chart = new SmoothieChart({
        minValue: 0.0,
        maxValue: 4096,
        grid: {
            millisPerLine: 1000,
            verticalSections: 6
        }
    });
    /* Stream the data to the graph. */
    chart.streamTo(document.getElementById("flex-sensor-graph"), 1000);
    /* Instantiate a TimSeries object to store x-values of graph. */
    const series = new TimeSeries();
    /* Add green line to graph from time series. */
    chart.addTimeSeries(series, { strokeStyle:'rgb(0,255,0)', lineWidth:2 });
    /* Instantiate a ServoUI object.*/
    new ServoUI(ws);
    /* Instantiate a FlexUI object. */
    new FlexUI(ws, chart);
});

/**
 * Enhanced websocket client. This class is responsible for dispatching
 *  device-related events to the UI, and communicating with the server.
 *
 * @param {string} url - The URL of the websocket server.
 */
class WSClient {
    constructor(url) {
        /* Instantiate a WebSocket object.*/
        this.ws = new WebSocket(url);
        /* Bind the _onMessage event handler to the onmessage event. */
        this.ws.onmessage = evt => this._onMessage(evt);
    }
    /*
            Events:

            eventName: UPDATE_SERVO,
            detail: 30

            eventName: UPDATE_FLEX,
            detail: {
                sensor: 2,
                reading: 1000
            };

            eventName: SERVO:ANGLE_STEP
            detail: 10


            eventName: FLEX
            detail: {
                item: "",
                value: 12
            };
     */
    /**
     * Handles incoming messages from the server.
     *
     * @param evt is the WebSocket message event.
     * @private
     */
    _onMessage(evt) {
        // Log event data
        console.log(evt.data);
        // Attempt to parse data as JSON.
        try {
            // Declare a JSON message to hold parsed data.
            const msg = JSON.parse(evt.data);
            /* Split message into components, defaulting to undefined for requests and statuses (they
                may sometimes be omitted if received GET requests. */
            const {dev, req = undefined, attr, val, stat = undefined} = msg;
            // Log parsed data.
            console.log(dev, req, attr, val, stat);
            // Dispatch events based on received data.
            switch (dev) {
                // Servo device.
                case 'SERVO':
                {
                    // Responses to get/set requests should always contain a value.
                    if (val === undefined) {
                        // Print an error if missing value.
                        console.warn(`Missing val field for servo. Message: ${msg}`);
                    // Otherwise, continue:
                    } else {
                        // Check if received was a servo position update.
                        if (attr === 'POSITION') {
                            // If so, dispatch an event to update servo position, with the details representing the position.
                            document.dispatchEvent(new CustomEvent("UPDATE_SERVO", {
                                detail: val,
                                bubbles: true
                            }));
                        // Otherwise, continue parsing attributes:
                        } else {
                            // Check if response was to a set request:
                            if (req === 'SET') {
                                // If so, check if the status was OK or ERROR.
                                if (stat === 'OK') {
                                    // Log ok status.
                                    console.log(`Server responded with OK to set servo's ${attr} to ${val}.`);
                                } else if (stat === 'ERROR') {
                                    // Log error status, retrieve actual value for attribute received.
                                    console.warn(`Server responded with ERROR to set servo's ${attr} to ${val}.`
                                        + `Sending GET request to retrieve last successful value.`);
                                    this.sendCommand(dev, 'GET', attr);
                                // If not get/set, log an error.
                                } else {
                                    console.warn(`No STAT field for received SET request: ${msg}`);
                                }
                            // Otherwise, check if received was a get request. If undefined, assume it's a get request.
                            } else if (req === 'GET' || req === undefined) {
                                // update UI based on attribute
                                switch (attr) {
                                    // one case for position
                                    case 'POSITION': document.dispatchEvent(new CustomEvent("SERVO", {detail: val})); break;
                                    // remaining attributes handled below
                                    case 'ANGLE_STEP':
                                    case 'TIME_DELAY':
                                    case 'MIN_PWM':
                                    case 'MAX_PWM':
                                    case 'PIN':
                                    case 'ACTUATE':
                                    case 'START_ANGLE':
                                    case 'STOP_ANGLE':
                                    case 'MOTION':
                                    case 'MAX_ANGLE':
                                    {
                                        // dispatch event with device and attribute, i.e., 'SERVO:ANGLE_STEP'
                                        document.dispatchEvent(new CustomEvent(`${dev}:${attr}`, {
                                            detail: val, // value received
                                            bubbles: true // bubble event up to UI
                                        }));
                                    } break;
                                    // not an attribute, warn
                                    default: console.warn(`Unknown servo attribute: ${attr}`);
                                }
                            // not get or set, so not a valid method. warn.
                            } else {
                                console.warn(`Unknown req '${req}' for SERVO. Received attribute wasn't POSITION.` +
                                `Received: ${msg}`);
                            }
                        }
                    }
                } break;
                // check if a static flex-sensor attribute (sampling rate)
                case 'FLEX':
                {
                    // make sure response has a value
                    if (val === undefined) {
                        // warn if not
                        console.warn(`Missing val field for flex. Message: ${msg}`);
                    // parse if so,
                    } else {
                        // check method, if set
                        if (req === 'SET') {
                            // set requests should always have a status. Check status, if OK,
                            if (stat === 'OK') {
                                // log ok
                                console.log(`Server responded with OK to set flex's ${attr} to ${val}.`);
                            // if not, warn,
                            } else if (stat === 'ERROR') {
                                console.warn(`Server responded with ERROR to set flex's ${attr} to ${val}.`
                                    + `Sending GET request to retrieve last successful value.`);
                                this.sendCommand(dev, 'GET', attr);
                            // if not either, warn of invalid status.
                            } else {
                                console.warn(`No STAT field for received SET request for dev FLEX: ${msg}`);
                            }
                        // if not set, check if get, assuming undefined is just a get response.
                        } else if (req === 'GET' || req === undefined) {
                            // check static attributes for flex sensor: sampling interval, start sampling, stop sampling.
                            if (attr === 'SAMPLE_RATE') {
                                // dispatch sampling rate get response to update dom
                                document.dispatchEvent(new CustomEvent("FLEX",
                                    {
                                        detail: {
                                            item: attr,
                                            value: val
                                        },
                                        bubbles: true
                                    }
                                ));
                                // check if start sampling
                            } else if (attr === 'START') {
                                document.dispatchEvent(new CustomEvent("FLEX", {
                                    detail: {
                                        item: attr,
                                        value: val
                                    },
                                    bubbles: true
                                }));
                                // check if stop sampling
                            } else if (attr === 'STOP') {
                                document.dispatchEvent(new CustomEvent("FLEX", {
                                    detail: {
                                        item: attr,
                                        value: val
                                    },
                                    bubbles: true
                                }));
                                // warn if not a valid static attribute
                            } else {
                                console.warn(`Unknown attr ${attr} for FLEX. Received: ${msg}`);
                            }
                            // not a valid method.
                        } else {
                            console.warn(`Unknown req '${req}' for FLEX. Received: ${msg}`);
                        }
                    }
                } break;
                // fall through for flex sensor devices
                case 'FLEX_2':
                case 'FLEX_3':
                case 'FLEX_4':
                case 'FLEX_5':
                {
                    // make sure response has a value
                    if (val === undefined) {
                        console.warn(`Missing val field for flex sensor. Message: ${msg}`);
                    } else {
                        // check if a reading response, dispatching the value
                        if (attr === 'READ') {
                            document.dispatchEvent(new CustomEvent("UPDATE_FLEX", {

                                detail: {
                                    /* store sensor and actual reading. split device string by '_',
                                        capturing 2nd element (index 1), interpreting as a base-10 number. */
                                    sensor: parseInt(dev.split('_')[1], 10),
                                    reading: val
                                },
                                bubbles: true
                            }));
                            // check if pin attribute, dispatching sensor-specific custom event
                        } else if (attr === 'PIN') {
                            document.dispatchEvent(new CustomEvent(`${dev}`, {
                                detail: {
                                    item: attr,
                                    value: val
                                }
                            }));
                            // warn invalid attribute.
                        } else {
                            console.warn(`Unknown attr ${attr} for flex sensor ${dev}. Received: ${msg}`);
                        }
                    }
                } break;
                // unknown device case, warn
                default: console.warn(`Unknown dev: ${dev}`); break;
            }
            // catch parsing errors, logging message.
        } catch (e) {
            console.warn("Bad JSON:", evt.data);
        }
    }

    /**
     * Send command to server.
     * @param dev is the device (String, SERVO/FLEX/FLEX_2/FLEX_3/FLEX_4/FLEX_5)
     * @param req is the request (String, GET/SET)
     * @param attr is the attribute (String, [servo attributes]/[flex static attr.]/[flex instance attr]
     * @param val is the value to send (String/Number — optional if GET request).
     */
    sendCommand(dev, req, attr, val) {
        // log command to send
        console.log(`Sending command: 
        {
            dev: "${dev}",
            req: "${req}",
            attr: "${attr}",
            val: ${val}
        };
        `);
        // define message
        const out = {dev, req, attr};
        // if SET request, add value field, otherwise don't
        if (val !== undefined) out.val = val;
        // send json object
        this.ws.send(JSON.stringify(out));
    }
}

/**
 * Class for managing servo UI components.
 *
 * @param ws is the WebSocket object.
 * @constructor instantiates reference to socket, defines UI components,
 * and binds event listeners to UI elements.
 *
 *
 */
class ServoUI {
    constructor(ws) {
        this.ws = ws;
        // define UI elements
        this.el = {
            angleStep: document.getElementById("SERVO ANGLE_STEP"),
            timeDelay: document.getElementById("SERVO TIME_DELAY"),
            position: document.getElementById("SERVO POSITION"),
            startAngle: document.getElementById("SERVO START_ANGLE"),
            stopAngle: document.getElementById("SERVO STOP_ANGLE"),
            motion: document.getElementById("SERVO MOTION"),
            actuateOn: document.getElementById("SERVO ACTUATE_ON"),
            actuateOff: document.getElementById("SERVO ACTUATE_OFF"),
            pin: document.getElementById("SERVO PIN"),
            minPwm: document.getElementById("SERVO MIN_PWM"),
            maxPwm: document.getElementById("SERVO MAX_PWM"),
            maxAngle: document.getElementById("SERVO MAX_ANGLE")
        };
        // bind angle notifier to position input
        document.addEventListener("UPDATE_SERVO", evt => {
            this.el.position.valueAsNumber = evt.detail;
        });
        // attribute is the second word in the id
        for (const element of Object.values(this.el)) {
            const attr = element.id.split(' ')[1];
            // bad formatting of id, warn
            if (attr === undefined) {
                console.warn('Unknown element in ServoUI w/ id: ', element.id);
                // go to next element
                continue;
            }
            /* Otherwise, bind dom event created from socket parsing to element (depending on element type, differentiated in anonymous) */
            document.addEventListener(`SERVO:${attr}`, evt => {
                if (element.type === 'select-one') {
                    element.value = evt.detail;
                } else if (element.type === 'number') {
                    element.valueAsNumber = evt.detail;
                } else {
                    console.warn(`Unknown element w/ id: ${element.id}`);
                }
            });
        }
        /* Bind element events to socket commands. */
        for (const element of Object.values(this.el)) {
            if (element.type === 'select-one') {
                element.addEventListener('change', evt => {
                    this.ws.sendCommand("SERVO", "SET",
                        evt.target.id.split(' ')[1], // attr field is second word in id
                        evt.target.value); // send value from event
                });
            } else if (element.type === 'number') {
                element.addEventListener('change', evt => {
                    this.ws.sendCommand("SERVO", 'SET', evt.target.id.split(' ')[1], evt.target.valueAsNumber);
                });
            } else if (element.id === 'SERVO ACTUATE_ON') {
                element.addEventListener('click', () => {
                    this.ws.sendCommand("SERVO", "SET", "ACTUATE", true);
                });
            } else if (element.id === 'SERVO ACTUATE_OFF') {
                element.addEventListener('click', () => {
                    this.ws.sendCommand("SERVO", "SET", "ACTUATE", false);
                });
            } else {
                console.warn(`Unknown element w/ id: ${element.id}`);
            }
        }
    }
}
class FlexUI {
    constructor(ws, graph) {
        this.ws = ws;
        this.graph = graph; // assign the smoothie chart
        this.maxVoltage = 3.3;
        this.series2 = new TimeSeries(); // time series for each sensor
        this.series3 = new TimeSeries();
        this.series4 = new TimeSeries();
        this.series5 = new TimeSeries();
        // add series to chart
        this.graph.addTimeSeries(this.series2, {strokeStyle:'rgb(255,0,0)', lineWidth:2 }); // index
        this.graph.addTimeSeries(this.series3, {strokeStyle:'rgb(0,255,0)', lineWidth:2 }); // middle
        this.graph.addTimeSeries(this.series4, {strokeStyle:'rgb(0,196,255)', lineWidth:2 }); // ring
        this.graph.addTimeSeries(this.series5, {strokeStyle:'rgb(255,98,0)', lineWidth:2 }); // pinky
        this.el = {
            pin2: document.getElementById("FLEX_2 PIN"),    // store all DOM elements within this class
            pin3: document.getElementById("FLEX_3 PIN"),
            pin4: document.getElementById("FLEX_4 PIN"),
            pin5: document.getElementById("FLEX_5 PIN"),
            reading2: document.getElementById("FLEX_2 READ"),
            reading3: document.getElementById("FLEX_3 READ"),
            reading4: document.getElementById("FLEX_4 READ"),
            reading5: document.getElementById("FLEX_5 READ"),
            fixed2: document.getElementById("FLEX_2 FIXED_RESIST"),
            fixed3: document.getElementById("FLEX_3 FIXED_RESIST"),
            fixed4: document.getElementById("FLEX_4 FIXED_RESIST"),
            fixed5: document.getElementById("FLEX_5 FIXED_RESIST"),
            volt2: document.getElementById("FLEX_2 VOLT"),
            volt3: document.getElementById("FLEX_3 VOLT"),
            volt4: document.getElementById("FLEX_4 VOLT"),
            volt5: document.getElementById("FLEX_5 VOLT"),
            resist2: document.getElementById("FLEX_2 RESIST"),
            resist3: document.getElementById("FLEX_3 RESIST"),
            resist4: document.getElementById("FLEX_4 RESIST"),
            resist5: document.getElementById("FLEX_5 RESIST"),
            start: document.getElementById('FLEX START'),
            stop: document.getElementById('FLEX STOP')
        };

        for (const element of Object.values(this.el)) {     // add event listeners to DOM, dispatching ws command events
            if (element.type === 'select-one') {            // must be a pin selector
                element.addEventListener('change', evt => { // graph the 0th element in array returned by splitting at space. send 'false' if NC, the pin no otherwise
                    // i got the pin numbers by holding mouse over the definitions of the macro constants and seeing their values (most reliable way)
                    this.ws.sendCommand(`${element.id.split(' ')[0]}`, 'SET', 'PIN', evt.target.value === 'false' ? 'false' : parseInt(evt.target.value));
                });
                //
            } else if (element.type === 'number') {
                element.addEventListener('change', evt => {
                    this.ws.sendCommand(`${element.id.split(' ')[0]}`, 'SET', 'PIN', parseInt(evt.target.value));
                });
            } else if (element.id === 'FLEX START') {
                element.addEventListener('click', () => {
                    this.ws.sendCommand("FLEX", "SET", "START", "true");
                    this.graph.start();
                });
            } else if (element.id === 'FLEX STOP') {
                element.addEventListener('click', () => {
                    this.ws.sendCommand("FLEX", "SET", "STOP", "");
                    this.graph.stop();
                });

            }
        }
        document.addEventListener("UPDATE_FLEX", evt => {
            const voltage = this.getVoltage(evt.detail.reading).toFixed(4);
            switch (evt.detail.sensor) {
                case 2: {
                    this.el.reading2.textContent = evt.detail.reading;
                    this.el.volt2.textContent = voltage;
                    this.el.resist2.textContent = this.getResistance(this.el.fixed2, voltage);
                    this.series2.append(Date.now(), evt.detail.reading);
                } break;
                case 3: {
                    this.el.reading3.textContent = evt.detail.reading;
                    this.el.volt3.textContent = voltage;
                    this.el.resist3.textContent = this.getResistance(this.el.fixed3, voltage);
                    this.series3.append(Date.now(), evt.detail.reading);
                } break;
                case 4:
                    this.el.reading4.textContent = evt.detail.reading;
                    this.el.volt4.textContent = voltage;
                    this.el.resist4.textContent = this.getResistance(this.el.fixed4, voltage);
                    this.series4.append(Date.now(), evt.detail.reading);
                    break;
                case 5:
                    this.el.reading5.textContent = evt.detail.reading;
                    this.el.volt5.textContent = voltage;
                    this.el.resist5.textContent = this.getResistance(this.el.fixed5, voltage);
                    this.series5.append(Date.now(), evt.detail.reading);
                    break;
                default: console.warn(`Unknown sensor: ${evt.detail.sensor}`);
                    break;
            }
        });
        document.addEventListener("FLEX", evt => {
            if (evt.detail.item === 'SAMPLE_RATE') {
                console.log(`Successfully received SAMPLE_RATE update request.`);
            } else if (evt.detail.item === 'START') {
                console.log(`Successfully received START update request.`);
            } else if (evt.detail.item === 'STOP') {
                console.log(`Successfully received STOP update request.`);
            } else {
                console.warn(`Unknown FLEX item: ${evt.detail}`);
            }
        });
        document.addEventListener("FLEX_2", evt => {
            if (evt.detail.item === 'PIN') {
                if (evt.detail.value === false) {
                    this.el.pin2.value = 'false';
                } else {
                    this.el.pin2.value = evt.detail.value;
                }
            } else {
                console.warn(`Unknown FLEX_2 item: ${evt.detail}`);
            }
        });
        document.addEventListener("FLEX_3", evt => {
            if (evt.detail.item === 'PIN') {
                if (evt.detail.value === false) {
                    this.el.pin3.value = 'false';
                } else {
                    this.el.pin3.value = evt.detail.value;
                }
            } else {
                console.warn(`Unknown FLEX_3 item: ${evt.detail}`);
            }
        });
        document.addEventListener("FLEX_4", evt => {
            if (evt.detail.item === 'PIN') {
                if (evt.detail.value === false) {
                    this.el.pin4.value = 'false';
                } else {
                    this.el.pin4.value = evt.detail.value;
                }
            } else {
                console.warn(`Unknown FLEX_4 item: ${evt.detail}`);
            }
        });
        document.addEventListener("FLEX_5", evt => {
            if (evt.detail.item === 'PIN') {
                if (evt.detail.value === false) {
                    this.el.pin5.value = 'false';
                } else {
                    this.el.pin5.value = evt.detail.value;
                }
            } else {
                console.warn(`Unknown FLEX_5 item: ${evt.detail}`);
            }
        });
    }
    // [voltage, resistance]
    getVoltage(adc) {
        return adc / 4095.0 * this.maxVoltage;
    }
    getResistance(fixedResist, voltage) {
        return fixedResist * voltage / (this.maxVoltage - voltage);
    }
}
