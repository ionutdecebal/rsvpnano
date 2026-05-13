var RSVPSharePreprocessor = function() {};

RSVPSharePreprocessor.prototype = {
  run: function(arguments) {
    arguments.completionFunction({
      URL: document.URL || "",
      title: document.title || "",
      selection: String(window.getSelection ? window.getSelection() : ""),
      HTML: document.documentElement ? document.documentElement.outerHTML : ""
    });
  },
  finalize: function(arguments) {}
};
