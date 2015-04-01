/*
    node-mcu-connect . node.js UDP Interface for embedded devices.
    Copyright (C) 2013-5 David Jakubowski
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. 
*/


var MCUObject = function(key) {

  this.children = [];
  this.key = key; 
  this.aliases = [];
  this.childType = "child";
  this.tags = [];

  this._selectors = [];

};

MCUObject.prototype._cacheSelector = function(selector,value) {

	this._selectors.push({"selector":selector,"value":value});
	return value;
}


/**
 * Function find(selector)
 * @returns 
 *
 * Transforms a selector to a collection of objects.
 */

MCUObject.prototype.find = function(selector) {

	var cached = _.find(this._selectors,{"selector":selector});
	if (cached) {
		return cached.value;	
	} 

  // split expression by spaces and recursively call ourselves
  // to reduce the result subset by subset
  var expression  = selector.split(' ');
  if (expression.length > 1) {
	 var tmp = this;
	 _.each(expression,function(item){
		tmp = tmp.find(item);
	 });
	 return this._cacheSelector(selector,tmp);
  }

  // treat simpler case where selector is just one string ([key]:[[tag]:[tag2]])
  var ret,children = [];
  var parseChildren = function(obj,filter,result) {
	 // check if the object matches, check also its children recursively
	 // when match, push result by reference
	 if (obj.children && obj.children.length) {
		result.push  (_.filter(obj.children,function(child){
			return  ((filter.key.length)?(child.key == filter.key):1)
					&& _.intersection(child.tags,filter.tags).length == filter.tags.length;
		}));
		_.each(obj.children,function(child) {
		  parseChildren(child,filter,result);
		});
	 } 
  }
  
  var oldSelector = selector;
  var selector = selector.split(":");
  tags = selector.splice(1);
  var key = selector;

  parseChildren(this,{"key":key[0],"tags":tags},children);
  children = _.flatten(children);

  if (children.length != 1) {
	 ret = new MCUObject();
	 ret.childType = "mixed-multiple";
	 ret.children = children;
  } else {
	 ret = children[0];
  }
  
  return this._cacheSelector(oldSelector,ret);

};

MCUObject.prototype.alias = function(alias) {
  this.aliases.push(alias);
};

MCUObject.prototype.add  = function(child) {
   
   child._network = this._network||this;
   this.children.push(child);
   return child;
};

MCUObject.prototype.tag  = function(tags) {
   
   this.tags = _.union(this.tags,tags.split(' '));
   return this;
};

MCUObject.prototype.on = function(eventId,fn) {

    if (this.childType == "mixed-multiple") {
       _.each(this.children,function(item){
           item.on(eventId,fn);
       });
    }

}

MCUObject.prototype.enable = function(val) {

    if (this.childType == "mixed-multiple") {
       _.each(this.children,function(item){
           item.enable(val);
       });
    }

}

