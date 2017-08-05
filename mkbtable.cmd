/* mkbtable.cmd �񕪌����ׂ̈̃e�[�u�������ׂ� REXX Script
 *   
 * �uA B�c�v�`���̍s�� A ���L�[�Ƃ��ă\�[�g���A
 * �u  { "A",B�c } �v�̌`�ŏo�͂���B
 */

n=0
DO WHILE LINES() > 0
    /* �^�u���󔒂ɕϊ����ǂݍ��� */
    line = TRANSLATE(linein(),' ','09'x)
    IF LEFT(line,1) = "#" | WORDS(line) < 2 THEN
	iterate
    n = n+1
    PARSE var line key.n dat.n
END
DO i=1 TO n-1
    DO j=i+1 TO n
	IF key.j < key.i THEN DO 
	    temp = key.i ; key.i = key.j ; key.j = temp
	    temp = dat.i ; dat.i = dat.j ; dat.j = temp
	END
    END
END

SAY "/* ���̃t�@�C���� mkbtable.cmd �ō��ꂽ���̂ł��B*/"
SAY "/* �{���e�ɕύX���s���ꍇ�̓\�[�X(*.tbl)�̕��ɕύX���K�v�ł��B*/"
SAY ""
DO i=1 TO n
    SAY '  { "' || key.i || '",' dat.i '},'
END



