CC      = cc
OBJS    = bto_ir_cmd.o

bto_ir_cmd: $(OBJS)
	$(CC) -Wall -o $@ $(OBJS) -lusb-1.0 

.c.o:
	$(CC) -c $<

clean:
	@rm -f $(OBJS)
	@rm -f bto_ir_cmd
