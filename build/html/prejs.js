Module.saveAs = Module.saveAs||function(e){"use strict";if(typeof e==="undefined"||typeof navigator!=="undefined"&&/MSIE [1-9]\./.test(navigator.userAgent)){return}var t=e.document,n=function(){return e.URL||e.webkitURL||e},r=t.createElementNS("http://www.w3.org/1999/xhtml","a"),o="download"in r,i=function(e){var t=new MouseEvent("click");e.dispatchEvent(t)},a=/constructor/i.test(e.HTMLElement),f=/CriOS\/[\d]+/.test(navigator.userAgent),u=function(t){(e.setImmediate||e.setTimeout)(function(){throw t},0)},d="application/octet-stream",s=1e3*40,c=function(e){var t=function(){if(typeof e==="string"){n().revokeObjectURL(e)}else{e.remove()}};setTimeout(t,s)},l=function(e,t,n){t=[].concat(t);var r=t.length;while(r--){var o=e["on"+t[r]];if(typeof o==="function"){try{o.call(e,n||e)}catch(i){u(i)}}}},p=function(e){if(/^\s*(?:text\/\S*|application\/xml|\S*\/\S*\+xml)\s*;.*charset\s*=\s*utf-8/i.test(e.type)){return new Blob([String.fromCharCode(65279),e],{type:e.type})}return e},v=function(t,u,s){if(!s){t=p(t)}var v=this,w=t.type,m=w===d,y,h=function(){l(v,"writestart progress write writeend".split(" "))},S=function(){if((f||m&&a)&&e.FileReader){var r=new FileReader;r.onloadend=function(){var t=f?r.result:r.result.replace(/^data:[^;]*;/,"data:attachment/file;");var n=e.open(t,"_blank");if(!n)e.location.href=t;t=undefined;v.readyState=v.DONE;h()};r.readAsDataURL(t);v.readyState=v.INIT;return}if(!y){y=n().createObjectURL(t)}if(m){e.location.href=y}else{var o=e.open(y,"_blank");if(!o){e.location.href=y}}v.readyState=v.DONE;h();c(y)};v.readyState=v.INIT;if(o){y=n().createObjectURL(t);setTimeout(function(){r.href=y;r.download=u;i(r);h();c(y);v.readyState=v.DONE});return}S()},w=v.prototype,m=function(e,t,n){return new v(e,t||e.name||"download",n)};if(typeof navigator!=="undefined"&&navigator.msSaveOrOpenBlob){return function(e,t,n){t=t||e.name||"download";if(!n){e=p(e)}return navigator.msSaveOrOpenBlob(e,t)}}w.abort=function(){};w.readyState=w.INIT=0;w.WRITING=1;w.DONE=2;w.error=w.onwritestart=w.onprogress=w.onwrite=w.onabort=w.onerror=w.onwriteend=null;return m}(typeof self!=="undefined"&&self||typeof window!=="undefined"&&window||this.content);if(typeof module!=="undefined"&&module.exports){module.exports.saveAs=saveAs}else if(typeof define!=="undefined"&&define!==null&&define.amd!==null){define([],function(){return saveAs})};
Module.showAddPopup = function(callback)
{
	var modal = document.getElementById('add-modal');
	var span = document.getElementsByClassName("close")[0];
	modal.style.display = "block";
	function cancel(){modal.style.display = "none"; callback(null, null);}
	span.onclick = cancel;
	window.onclick = function(event) {if (event.target == modal) cancel();}

	var uploadInput = document.getElementById('upload-input');
	uploadInput.onchange = function()
	{
		var file = uploadInput.files[0];

		if(!file) return;

		var reader = new FileReader();

		reader.onload = function(event)
		{
			var rom = new Uint8Array(event.target.result);

			callback(file.name, rom);

			uploadInput.value = "";
			modal.style.display = "none";
		};

		reader.readAsArrayBuffer(file);
	};
};
// The safe-area insets are only readable from CSS, and they change with the
// orientation and with the browser's own bars, so the page pushes them in
// whenever the viewport moves. The probe element is what makes env() readable.
Module.tic80Viewport = function()
{
	if(!Module._tic80_insets) return;

	var probe = Module.tic80Probe;

	if(!probe)
	{
		probe = document.createElement('div');
		probe.style.cssText = 'position:fixed;left:0;top:0;visibility:hidden;' +
			'padding:env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left)';
		document.body.appendChild(probe);
		Module.tic80Probe = probe;
	}

	var style = getComputedStyle(probe);
	var value = function(name) { return parseFloat(style.getPropertyValue(name)) || 0; };

	Module._tic80_insets(value('padding-top'), value('padding-right'), value('padding-bottom'), value('padding-left'));
};

(function()
{
	Module.tic80Viewport();
	addEventListener('resize', Module.tic80Viewport);
	addEventListener('orientationchange', Module.tic80Viewport);

	// The iOS URL bar and the soft keyboard move the visual viewport without a
	// window resize.
	if(window.visualViewport)
	{
		visualViewport.addEventListener('resize', Module.tic80Viewport);
		visualViewport.addEventListener('scroll', Module.tic80Viewport);
	}
})();
