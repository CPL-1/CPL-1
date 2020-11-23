import inotify.adapters
import os.path
import os

inotifier = inotify.adapters.InotifyTree('.')
extensions = ['c', 'asm']
object_file_extension = 'o'

for event in inotifier.event_gen():
    if event is not None:
        (_, type_names, path, filename) = event
        if 'IN_CLOSE_WRITE' not in type_names:
            continue
        fullpath = path + '/' + filename
        extension = os.path.splitext(filename)[1][1:]
        print('Modified file', fullpath, 'Extension:', extension)
        if extension not in extensions:
            continue
        object_file_path = os.path.splitext(fullpath)[0] + '.' + object_file_extension
        if os.path.exists(object_file_path):
            os.remove(object_file_path)
