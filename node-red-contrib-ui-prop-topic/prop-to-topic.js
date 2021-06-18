module.exports = function(RED) {
    function PropToTopicNode(config) {
        RED.nodes.createNode(this,config);
        this.prop = config.prop;
        var node = this;
        node.on('input', function(msg) {
            var curVal = msg.payload;
            var propSplit = node.prop.replace(/\]/g,'').split(/[\[\.]/);
            for (let i = 0; i < propSplit.length; i++) {
                if (curVal === null || typeof curVal === 'undefined') {
                    curVal = null;
                    break;
                } 
                var propname = propSplit[i];
                curVal = curVal[propname];                
            }
            msg.origVal = msg.payload;
            msg.payload = curVal;
            msg.topic = node.prop;            
            node.send(msg);
        });
    }
    RED.nodes.registerType("prop-to-topic",PropToTopicNode);
}