import pygtk
pygtk.require('2.0')
import gtk

import application

class PrerequisitesAdderController :
	
	def __init__(self ) :
		self.view = gtk.glade.XML('glade-files/widgets/widgets.glade', 'add_prerequisites_dlg' )
		self.dialog = self.view.get_widget('add_prerequisites_dlg')
		self.tech_tree = gtk.TreeStore(str,str)

		self.__setup_techs_list_view()

	def __setup_techs_list_view( self ) :
		techs_list = self.view.get_widget('tech_browser')

		identifier_column = gtk.TreeViewColumn('Identifier')
		techs_list.append_column( identifier_column )
		cell = gtk.CellRendererText()
		identifier_column.pack_start( cell, True )
		identifier_column.add_attribute(cell, 'text', 0)		
		identifier_column.set_sort_column_id(0)

		name_column = gtk.TreeViewColumn('Name')
		techs_list.append_column( name_column )	
		cell = gtk.CellRendererText()
		name_column.pack_start( cell, True )
		name_column.add_attribute( cell, 'text', 1)

		techs_list.set_search_column(0)

	def __read_tech_list( self ) :

		self.view.get_widget('tech_browser').set_model(None)
		self.tech_tree.clear()
		tech_set = application.instance.tech_set
		english_strings = application.instance.supported_languages['English']
		
		for category_identifier, tech_list in tech_set.tech_categories.items() :
			try:
				piter = self.tech_tree.append( None, (category_identifier, english_strings[category_identifier]) )
			except KeyError :
				piter = self.tech_tree.append( None, (category_identifier, "") )
			for tech_name in  tech_list :
				tech = tech_set.tech_entries[tech_name]
				try:	
					self.tech_tree.append( piter, (tech.name, english_strings[tech.name]) )
				except KeyError :
					self.tech_tree.append( piter, (tech.name, "") )
		
		self.view.get_widget('tech_browser').set_model(self.tech_tree)		

	def execute( self ) :
		self.__read_tech_list() 
		selected_techs = []
		response = self.dialog.run()

		if response == gtk.RESPONSE_OK : # techs selected
			tech_list = self.view.get_widget('tech_browser')
			_, paths = tech_list.get_selection().get_selected_rows()
			
			if len( paths ) >  0 :
				for path in paths :
					iter = self.tech_tree.get_iter(path)
					tech_id = self.tech_tree.get_value(iter,0)
					selected_techs.append(tech_id)

		self.dialog.hide()
		self.dialog.destroy()

		return selected_techs

