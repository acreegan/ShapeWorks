from DocumentationUtils import CommandsUtilsfrom mdutils.mdutils import MdUtils#%%def generateShapeWorksCommandDocumentation(mdFilename = '../../Documentation/ShapeWorksCommands/ShapeWorksCommands.md'):    # settings from Executable.cpp    opt_width  = 32    indent     = 2    spacedelim = ''.ljust(indent)        mdFile     = MdUtils(file_name = mdFilename, title = 'ShapeWorks Commands')    cmd        =  "shapeworks"    CommandsUtils.addCommand(mdFile, cmd, level = 1, spacedelim = spacedelim, verbose = True)    mdFile.new_table_of_contents(table_title='Contents', depth=2)    mdFile.create_md_file()    #%%# docPath  = '/Users/shireen/MyStuff/PROJECTS/SHAPEWORKS_DEVELOPMENT/TOOLS/ShapeWorks-exec-auto-doc/Documentation/ShapeWorksCommands/'# mdFilename = docPath + 'shapeworks_exec'