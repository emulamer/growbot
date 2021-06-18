module.exports = function(RED) {
    function TopicToPropNode(config) {
        RED.nodes.createNode(this,config);
        this.topic = config.topic;
        var node = this;
        node.on('input', function(msg) {
            var newVal = msg.payload;
            var origObj = msg.origVal;
            var curVal = msg.origVal;
            if (!curVal) {
                throw "Didn't find original object to set";
            } 
            var propSplit = msg.topic.replace(/\]/g,'').split(/[\[\.]/);
            if (propSplit.length == 1) {
                curVal[propSplit[0]] = newVal;
            } else {
                for (let i = 0; i < propSplit.length - 1; i++) {
                    var propname = propSplit[i];
                    console.log(propname);
                    if (curVal === null || typeof curVal === 'undefined') {
                        curVal = null;
                        break;
                    }
                    
                    curVal = curVal[propname];                
                }
                if (curVal !== null && typeof curVal !== 'undefined') {
                    curVal[propSplit[propSplit.length-1]] = newVal;
                }
            }
            delete msg.origVal;
            msg.payload = origObj;
            msg.topic = node.topic;       
            node.send(msg);
        });
    }
    RED.nodes.registerType("topic-to-prop",TopicToPropNode);
}