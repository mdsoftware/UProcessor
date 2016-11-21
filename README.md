UProcessor Overview
===================
Shortly, this is a compiler which is compile script code in RPN byte code, and allow to execute such byte code assuming ‘untyped’ (usual) variables and appropriate memory manager.

Functions cannot be declared in this code but can be supplied externally as a pointers.

This code was written from a scratch ‘just for fun’, but then it was successfully included in some business solutions. For now I can describe such solutions as BPMs.

Examples of code (in actual life it works like PHP):

```
<$
  MCatTitle = SQLToValue('select [Title] from [MasterCategory] where [MasterCategoryID]=' + NTrim(Parent.MCatID))
  if !IsString(MCatTitle)
    Fatal('Error getting master category title')
  end
  CatTitle = SQLToValue('select [Title] from [Category] where [CategoryID]=' + NTrim(Parent.CatID))
  if !IsString(CatTitle)
    Fatal('Error getting category title')
  end
  PList = SQLToArray('SELECT [Product].*  from [Product] where ([Product].[ProductID] in ' + ;
   '(select [MultipleCategory].[ProductID] from [MultipleCategory] where [MultipleCategory].[CategoryID]=' + ;
   NTrim(Parent.CatID) + ')) or ([Product].[CategoryID]=' + NTrim(Parent.CatID) + ') order by [Product].[Name]')
  if !IsArray(PList)
    Fatal('Error getting product list')
  end
  // <td align="center"><span class="hdr2" align="center">Prod. 56</span><br><span class="hdr2" align="center"><b>The Luxemburg Gardens</b></span><br><a href="The-Luxemburg-Gardens-56.htm"><img border="0" alt="Art tapestries / The Luxemburg Gardens" src="../../store/productimages/small/The-Luxemburg-Gardens.jpg" onError=src="../images/ImageComingSoon.gif"></a></td>
$>
<table><tr><td colspan="2">&nbsp;</td></tr>
<tr><td width="596">

<font color="#004148"><strong><a href=<$ Print('"', ToPageName(MCatTitle), '.htm"') $>><$ Print(MCatTitle) $>:</a> <$ Print(CatTitle) $></strong></font><br><br>
<table width="100%" border="0" cellspacing="0" cellpadding="0"><tr><$
  i = 1
  c = 0
  nextrow = ''
  while i <= Len(PList)
    Rec = PList[i] 

    Print('<td align="center">Prod. #BT', NTrim(Rec.ProductID), '<br><strong>',;
       Rec.Name, '</strong><br><a href="', ToPageName(Rec.ShortName, NTrim(Rec.ProductID)),;
       '.htm"><img border="0" alt="',MCatTitle, ' / ', Rec.Name, '" src="../productimages/',Rec.ThumbImage,;
       '" onError=src="../images/ImageComingSoon.gif"></a></td>')
    nextrow = nextrow + ;
      '<td align="center"><img alt="Free Shipping" src="../images/freeshipping.jpg" width="80" height="21"><img alt="On Sale" src="../images/sale.gif" width="37" height="35"><br>&nbsp;</td>' + ;
      char(13) + char(10)
    Print(char(13)+char(10))
    c = c + 1
    if c == 3
      Print('</tr><tr>', nextrow, '</tr><tr>', char(13)+char(10))
      nextrow = ''
      c = 0
    end
    i = i + 1
  end$></tr><$ Print('<tr>', nextrow, '</tr>') $></table>
```




