import pygtk
pygtk.require('2.0')
import gtk
import gtk.glade
import gtk.gdk

from tools import not_implemented_feature_dialog, recoverable_error
from application.techs import Tech
import application
from prereq_adder import *

class AddTechDlgController :

	def __init__( self ) :
		self.view = gtk.glade.XML( 'glade-files/widgets/widgets.glade','add_tech_dlg')
		self.dialog = self.view.get_widget('add_tech_dlg')

		# signal connection
		self.view.signal_connect('new_tech_on_add_prerequisite_clicked', self.__on_add_prerequisite )
		self.view.signal_connect('new_tech_on_remove_prerequisite_clicked', self.__on_remove_prerequisites )

		self.view.signal_connect('on_new_tech_type_combo_box_changed', self.__on_tech_type_changed )
		self.view.signal_connect('on_new_tech_category_combo_box_changed', self.__on_tech_category_changed )

		self.prereq_list = gtk.ListStore(str,str)
		self.items_list = gtk.ListStore(str,str)
		self.effects_list = gtk.ListStore(str,str)

		self.__setup_prerequisites_view()
		self.__setup_unlocked_items_view()
		self.__setup_effects_view()

		# combo-boxes setup
		self.tech_types_list = gtk.ListStore(str)
		self.tech_categories_list = gtk.ListStore(str)
		self.__setup_tech_types_combo_box()
		self.__setup_tech_categories_combo_box()

		self.desc_text_buffer = gtk.TextBuffer()
		self.desc_text_buffer.set_text("")
		desc_str_txt = self.view.get_widget('new_tech_description_text_view')
				

		tech_image = self.view.get_widget('new_tech_icon')
		icon_path = 'no_image.png'
		tech_image.set_from_pixbuf( gtk.gdk.pixbuf_new_from_file(icon_path)	 )

		self.current_tech = None

	def __on_add_prerequisite( self, button, *args ) :
		if self.current_tech is None : return
		print "Adding prerequisites..."

		dialog = PrerequisitesAdderController()		

		techs_to_add = dialog.execute()

		if len(techs_to_add) == 0 : return
	
		for tech in techs_to_add :
			try:
				self.current_tech.prerequisites.index(tech)
				continue
			except ValueError :
				self.current_tech.prerequisites.append(tech)
				self.current_tech.modified = True

		self.__display_prerequisites()


	def __on_remove_prerequisites( self, button, *args ) :
		if self.current_tech is None : return

		print "Removing a prerequisite"
		prereq_view = self.view.get_widget('new_tech_prereq_list')
		_, paths = prereq_view.get_selection().get_selected_rows()
		techs_to_remove = []
		if len(paths) > 0 :
			for path in paths :
				iter = self.prereq_list.get_iter(path)
				tech_id = self.prereq_list.get_value(iter,0)
				self.current_tech.prerequisites.remove(tech_id)
			
			self.current_tech.modified = True
			self.__display_prerequisites()

	def __on_tech_category_changed( self, combobox, *args ) :
		if self.current_tech is None: return
		
		iter = combobox.get_active_iter()

		# This is necessary because there seems to be a bug in
		# PyGtk that gets the changed signal emitted thrice:
		# once with an iter, and twice with a null iter...
		if iter is None : return

		selected_category = self.tech_categories_list.get_value(iter,0)
		combobox.child.set_text(selected_category)
		self.current_tech.category = selected_category
	
		strings = application.instance.supported_languages['English']
		category_str_txt = self.view.get_widget('new_tech_category_str_entry')
		try:
			category_str_txt.set_text( strings[self.current_tech.category] )
		except KeyError :
			pass


	def __on_tech_type_changed( self, combobox, *args ) :
		if self.current_tech is None: return
		
		iter = combobox.get_active_iter()

		# This is necessary because there seems to be a bug in
		# PyGtk that gets the changed signal emitted thrice:
		# once with an iter, and twice with a null iter...
		if iter is None : return

		selected_type = self.tech_types_list.get_value(iter,0)
		combobox.child.set_text(selected_type)
		self.current_tech.type = selected_type
	
		strings = application.instance.supported_languages['English']
		type_str_txt = self.view.get_widget('new_tech_type_str_entry')
		try:
			type_str_txt.set_text( strings[self.current_tech.type] )
		except KeyError :
			pass


	def __setup_tech_types_combo_box( self ) :
		tech_type_combo = self.view.get_widget( 'new_tech_type_combo' )	
		tech_type_combo.set_model(None)
		cell = gtk.CellRendererText()
		tech_type_combo.pack_start(cell, True)
		tech_type_combo.add_attribute(cell, 'text', 0)
		for type in Tech.types :
			self.tech_types_list.append( (type,) ) 		

		tech_type_combo.set_model(self.tech_types_list)

	def __setup_tech_categories_combo_box( self ) :
		tech_cat_combo = self.view.get_widget( 'new_tech_category_combo' )
		tech_cat_combo.set_model( None )
		cell = gtk.CellRendererText()
		tech_cat_combo.pack_start(cell, True)
		tech_cat_combo.add_attribute(cell, 'text', 0)
		tech_set = application.instance.tech_set
		for cat_identifier in tech_set.tech_categories.keys() :
			self.tech_categories_list.append( (cat_identifier,) )		

		tech_cat_combo.set_model(self.tech_categories_list)


	def __display_prerequisites( self ) :
		english_strings = application.instance.supported_languages['English']

		prereq_view = self.view.get_widget('new_tech_prereq_list')
		prereq_view.set_model(None)
		self.prereq_list.clear()
		for prereq in self.current_tech.prerequisites :
			try:
				self.prereq_list.append((prereq,english_strings[prereq]))
			except KeyError :
				self.prereq_list.append((prereq,""))
					
		prereq_view.set_model(self.prereq_list)


	def __setup_prerequisites_view( self ) :
		prereqs_list = self.view.get_widget('new_tech_prereq_list')

		identifier_column = gtk.TreeViewColumn('Identifier')
		prereqs_list.append_column( identifier_column )
		cell = gtk.CellRendererText()
		identifier_column.pack_start( cell, True )
		identifier_column.add_attribute(cell, 'text', 0)		
		identifier_column.set_sort_column_id(0)

		name_column = gtk.TreeViewColumn('Name')
		prereqs_list.append_column( name_column )	
		cell = gtk.CellRendererText()
		name_column.pack_start( cell, True )
		name_column.add_attribute( cell, 'text', 1)

		prereqs_list.set_search_column(0)
	
	def __setup_unlocked_items_view( self ) :
		items_list = self.view.get_widget('new_tech_items_list')

		identifier_column = gtk.TreeViewColumn('Identifier')
		items_list.append_column( identifier_column )
		cell = gtk.CellRendererText()
		identifier_column.pack_start( cell, True )
		identifier_column.add_attribute(cell, 'text', 0)		
		identifier_column.set_sort_column_id(0)

		name_column = gtk.TreeViewColumn('Name')
		items_list.append_column( name_column )	
		cell = gtk.CellRendererText()
		name_column.pack_start( cell, True )
		name_column.add_attribute( cell, 'text', 1)

		items_list.set_search_column(0)
	
	def __setup_effects_view( self ) :
		effects_list = self.view.get_widget('new_tech_effects_list')

		identifier_column = gtk.TreeViewColumn('Identifier')
		effects_list.append_column( identifier_column )
		cell = gtk.CellRendererText()
		identifier_column.pack_start( cell, True )
		identifier_column.add_attribute(cell, 'text', 0)		
		identifier_column.set_sort_column_id(0)

		name_column = gtk.TreeViewColumn('Name')
		effects_list.append_column( name_column )	
		cell = gtk.CellRendererText()
		name_column.pack_start( cell, True )
		name_column.add_attribute( cell, 'text', 1)

		effects_list.set_search_column(0)

	def __get_data_from_widgets( self ) :
		field_values = []
		strings = dict()

		tech_name_id = 	self.view.get_widget('new_tech_name_entry').get_text()
		field_values.append(( 'name', tech_name_id ))
		strings[tech_name_id] = self.view.get_widget('new_tech_name_str_entry').get_text()
		
		tech_type_id = self.view.get_widget('new_tech_type_combo').child.get_text()
		field_values.append(( 'type', tech_type_id ))

		tech_cat_id = self.view.get_widget('new_tech_category_combo').child.get_text()
		field_values.append(( 'category', tech_cat_id ))
		
		tech_cost = self.view.get_widget('new_tech_research_cost_button').get_value_as_int()
		field_values.append(('research_cost', tech_cost))
		
		tech_turns = self.view.get_widget('new_tech_research_turns_button').get_value_as_int()
		field_values.append(('research_turns', tech_turns))

		tech_desc_id = tech_name_id + '_DESC'
		field_values.append(('description',tech_desc_id))
		text_start, text_end = self.desc_text_buffer.get_start_iter(), self.desc_text_buffer.get_end_iter()
		strings[tech_desc_id] = self.desc_text_buffer.get_text(text_start, text_end)		

		return field_values, strings

	def __validate_tech( self ) :

		if len(self.current_tech.name) == 0 :
			error = RuntimeError()
			error.user_message = "Tech is unnamed. Please specify a tech identifier"
			raise error

		try:
			strings = application.instance.supported_languages['English']
			name = strings[self.current_tech.name]
					
			error = RuntimeError()
			error.user_message = "Tech name %s is already in use. Please choose a different name."%self.current_tech.name
			raise error
		except KeyError :
			pass

		if self.current_tech.type is None or len(self.current_tech.type) == 0 :
			error = RuntimeError()
			error.user_message = "Tech cannot be typeless. Please select a from the list."
			raise error

		if self.current_tech.category is None or len(self.current_tech.category) == 0 :
			error = RuntimeError()
			error.user_message = "Tech must belong to a category. Please select one from the list."
			raise error



	def __try_to_add_new_tech( self ) :
		# get data from the widgets
		field_list, new_strings = self.__get_data_from_widgets()
		
		for attname, value in field_list :
			setattr( self.current_tech, attname, value )

		self.current_tech.strings = new_strings

		self.__validate_tech()

		application.instance.tech_set.add_new_tech(self.current_tech)
		application.instance.techs_modified = True
		print "Tech added"
		print self.current_tech.to_xml()


	def execute( self ) :
		self.current_tech = Tech()
		done = False				
		tech_created = False
	
		while not done :
			response = self.dialog.run()
		
			if response == gtk.RESPONSE_OK : # create tech
				try:
					self.__try_to_add_new_tech()
					done = True
					tech_created = True
				except RuntimeError, e :
					recoverable_error(self.dialog,e.user_message)
			else :
				done, tech_created = True, False
		
		self.dialog.hide()
		self.dialog.destroy()
	
		return tech_created
		
		
