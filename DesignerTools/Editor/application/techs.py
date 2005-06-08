import sys, os, re, copy
from xml.dom import minidom
import application, strtable

def get_child_data( parent ) :
	result =  ''.join([node.data for node in parent.childNodes])
	return result.strip()


class Tech :
	
	types = ['TT_THEORY','TT_APPLICATION','TT_REFINEMENT']

	def __init__(self) :
		self.name = "DUMMY_TECH"
		self.description = "DUMMY_DESCRIPTION"
		self.type = "DUMMY_TYPE"
		self.category = "DUMMY_CATEGORY"
		self.research_cost = 1
		self.research_turns = 1
		self.prerequisites = []
		self.unlocked_items = []
		self.effects = []
		self.graphic = None
		self.modified = False

	def copy( self ) :
		return copy.deepcopy(self)

	def to_xml( self ) :
		strings = []

		strings.append( '<Tech>' )
		strings.append( '<name>%s</name>'%self.name )
		strings.append( '<description>%s</description>'%self.description )
		strings.append( '<type>%s</type>'%self.type )
		strings.append( '<category>%s</category>'%self.category )
		strings.append( '<research_cost>%s</research_cost>'%self.research_cost )
		strings.append( '<research_turns>%s</research_turns>'%self.research_turns )

		items_strings = []
		if len(self.prerequisites) == 0 :
			strings.append( '<prerequisites/>' )
		else :		
			strings.append( '<prerequisites>' )
			for prereq in self.prerequisites :
				items_strings.append( '<prereq>%s</prereq>'%prereq )
			strings.append( '\n'.join(items_strings) )
			strings.append( '</prerequisites>' )

		if len( self.unlocked_items ) == 0 :
			strings.append( '<unlocked_items/>' )
		else :
			strings.append( '<unlocked_items>' )
			items_strings = []
			for item in self.unlocked_items :
				items_strings.append( '<item>%s</item>'%item )
			strings.append( '\n'.join(items_strings) )
			strings.append( '</unlocked_items>' )

		if len( self.effects ) == 0 :
			strings.append( '<effects/>' )
		else :
			strings.append( '<effects>' )
			items_strings = []
			for effect in self.effects :
				items_strings.append( '<effect>%s</effect>'%effect )
			strings.append( '\n'.join(items_strings) )
			strings.append( '</effects>' )

		if ( self.graphic is None ) :
			strings.append( '<graphic/>')
		else :
			strings.append( '<graphic>%s</graphic>'%self.graphic )
		strings.append( '</Tech>' )
		return '\n'.join(strings)

	def load_from_xml_node( klass, xml_node ) :
		load_tech = Tech()

		strings = ['name','description','type','category','graphic']
		
		children = [ xml_node.getElementsByTagName(tag_name) for tag_name in strings  ]
		data = []
		for i in range(len(children)) :
			if len(children[i]) == 0 :
					data.append(None)
			else :
				data.append(get_child_data( children[i][0] ))

		assignments = zip( strings, data )

		for args in assignments :
			setattr(load_tech, *args )

		integers = ['research_cost','research_turns']

		children = [ xml_node.getElementsByTagName(tag_name) for tag_name in integers  ]
		data = []
		for i in range(len(children)) :
			if children[i] == None :
				data.append( 1 )
			else :
				data.append( int(get_child_data( children[i][0] )) )

		assignments = zip( integers, data )

		for args in assignments :
			setattr(load_tech, *args )
		
		lists = [('unlocked_items','item'),('prerequisites','prereq'),('effects','effect')]
		children = [ xml_node.getElementsByTagName(tag_name) for tag_name,_ in lists ]

		for i in range( len(children) ) :
			if len(children[i]) > 0 :
				elem_nodes = children[i][0].getElementsByTagName(lists[i][1])
				for elem_node in elem_nodes :
					getattr( load_tech, lists[i][0] ).append(get_child_data(elem_node))

		#print "\tTech",load_tech.name,"loaded..."

		return load_tech

	load_from_xml_node = classmethod(load_from_xml_node)

class GameTechs :
		
	def __init__( self ) :
		self.tech_entries = dict()
		self.tech_categories = dict()
		self.tech_categories["DUMMY_CATEGORY"] = []

	def add_new_category( self, key ) :
		self.tech_categories[key] = []
	
	def add_new_tech( self, tech ) :
		self.tech_entries[tech.name] = tech
		self.tech_categories[tech.category].append(tech.name)

		try:

			new_strings = tech.strings
			strings = application.instance.supported_languages['English']
			strtable.add_entries( strings, new_strings )	

			application.instance.strings_modified = True
		except AttributeError :
			pass

	def remove_tech( self, tech_id ) :
		tech = self.tech_entries[tech_id]

		strings_for_removal = [tech.name,tech.description]
		strings = application.instance.supported_languages['English']
		strtable.remove_entries( strings, strings_for_removal )	

		del self.tech_entries[tech.name]				
		self.tech_categories[tech.category].remove(tech.name)	

	def store_techs( self, basename, path ) :
		filename = basename + '/' + path
		techs_stream = open( filename, 'w' )

		print >> techs_stream, '<?xml version="1.0" encoding="UTF-8" ?>'
		print >> techs_stream, '<GG::XMLDoc>'
		
		for category, tech_names in self.tech_categories.items() :
			if category == 'DUMMY_CATEGORY' : continue
			print >> techs_stream, '<Category>%s</Category>'%category
			for tech_name in tech_names :
				print >> techs_stream, self.tech_entries[tech_name].to_xml()

		print >> techs_stream, '</GG::XMLDoc>'

		techs_stream.close()

def load_techs_descriptions( basename, path ) :
	path = basename + '/' + path
	
	tech_stream = open( path )
	text = tech_stream.read()
	tech_stream.close()
	
	text = text.replace( 'GG::XMLDoc', 'GG_XMLDoc' )

	xml_document = minidom.parseString(text)

	print "File", path, "was parsed successfully!"

	category_nodes = xml_document.getElementsByTagName('Category')

	techs = GameTechs()

	for category_node in category_nodes :		
		category_key = get_child_data( category_node )
		techs.add_new_category( category_key )

	print "Found",len(techs.tech_categories.keys()),"tech categories"

	tech_nodes = xml_document.getElementsByTagName('Tech')

	for tech_node in tech_nodes :
		tech = Tech.load_from_xml_node( tech_node )
		techs.add_new_tech(tech)

	print "Loaded", len(techs.tech_entries), "techs"

	return techs 
	
