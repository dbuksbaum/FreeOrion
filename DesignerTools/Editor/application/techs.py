import sys, os, re
from xml.dom import minidom

def get_child_data( parent ) :
	result =  ''.join([node.data for node in parent.childNodes])
	return result.strip()


class Tech :

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

	def load_from_xml_node( klass, xml_node ) :
		load_tech = Tech()

		strings = ['name','description','type','category','graphic']
		
		children = [ xml_node.getElementsByTagName(tag_name) for tag_name in strings  ]
		data = []
		for i in range(len(children)) :
			if len(children[i]) == 0 :
				if strings[i] == 'graphic' :
					data.append("no_image.png")
				else :
					data.append("DUMMY_"+strings[i].upper())
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

		print "\tTech",load_tech.name,"loaded..."

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
		self.tech_categories[tech.category].append(tech)

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
	
