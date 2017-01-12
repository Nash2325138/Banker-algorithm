import re
fin = open('log.txt', 'r')
txt = fin.read()
ansi_escape = re.compile(r'\x1b[^m]*m')
colorless = ansi_escape.sub('', txt)

fout = open('log_no_color.txt', 'w')
fout.write(colorless)
