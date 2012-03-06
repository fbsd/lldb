# summary provider for NSBundle
import lldb
import ctypes
import objc_runtime
import metrics
import NSURL

statistics = metrics.Metrics()
statistics.add_metric('invalid_isa')
statistics.add_metric('invalid_pointer')
statistics.add_metric('unknown_class')
statistics.add_metric('code_notrun')

# despite the similary to synthetic children providers, these classes are not
# trying to provide anything but a summary for an NSURL, so they need not
# obey the interface specification for synthetic children providers
class NSBundleKnown_SummaryProvider:
	def adjust_for_architecture(self):
		pass

	def __init__(self, valobj, params):
		self.valobj = valobj;
		self.sys_params = params
		if not(self.sys_params.types_cache.NSString):
			self.sys_params.types_cache.NSString = self.valobj.GetTarget().FindFirstType('NSString').GetPointerType()
		self.update();

	def update(self):
		self.adjust_for_architecture();

	# we need to skip the ISA, plus four other values
	# that are luckily each a pointer in size
	# which makes our computation trivial :-)
	def offset(self):
		return 5 * self.sys_params.pointer_size

	def url_text(self):
		global statistics
		text = self.valobj.CreateChildAtOffset("text",
							self.offset(),
							self.sys_params.types_cache.NSString)
		my_string = text.GetSummary()
		if (my_string == None) or (my_string == ''):
			statistics.metric_hit('unknown_class',str(self.valobj) + " triggered unkown pointer location")
			return NSBundleUnknown_SummaryProvider(self.valobj, self.sys_params).url_text()
		else:
			statistics.metric_hit('code_notrun',self.valobj)
			return my_string


class NSBundleUnknown_SummaryProvider:
	def adjust_for_architecture(self):
		pass

	def __init__(self, valobj, params):
		self.valobj = valobj;
		self.sys_params = params
		self.update()

	def update(self):
		self.adjust_for_architecture();

	def url_text(self):
		stream = lldb.SBStream()
		self.valobj.GetExpressionPath(stream)
		expr = "(NSString*)[" + stream.GetData() + " bundlePath]"
		url_text_vo = self.valobj.CreateValueFromExpression("path",expr);
		return url_text_vo.GetSummary()


def GetSummary_Impl(valobj):
	global statistics
	class_data = objc_runtime.ObjCRuntime(valobj)
	if class_data.is_valid() == False:
		statistics.metric_hit('invalid_pointer',valobj)
		wrapper = None
		return
	class_data = class_data.read_class_data()
	if class_data.is_valid() == False:
		statistics.metric_hit('invalid_isa',valobj)
		wrapper = None
		return
	if class_data.is_kvo():
		class_data = class_data.get_superclass()
	if class_data.is_valid() == False:
		statistics.metric_hit('invalid_isa',valobj)
		wrapper = None
		return
	
	name_string = class_data.class_name()
	if name_string == 'NSBundle':
		wrapper = NSBundleKnown_SummaryProvider(valobj, class_data.sys_params)
		# [NSBundle mainBundle] does return an object that is
		# not correctly filled out for our purposes, so we still
		# end up having to run code in that case
		#statistics.metric_hit('code_notrun',valobj)
	else:
		wrapper = NSBundleUnknown_SummaryProvider(valobj, class_data.sys_params)
		statistics.metric_hit('unknown_class',str(valobj) + " seen as " + name_string)
	return wrapper;

def NSBundle_SummaryProvider (valobj,dict):
	provider = GetSummary_Impl(valobj);
	if provider != None:
	    try:
	        summary = provider.url_text();
	    except:
	        summary = None
	    if summary == None or summary == '':
	        summary = 'no valid NSBundle here'
	    return summary
	return ''

def __lldb_init_module(debugger,dict):
	debugger.HandleCommand("type summary add -F NSBundle.NSBundle_SummaryProvider NSBundle")