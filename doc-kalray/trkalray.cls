% *********************************************************
% * Class file for the Electronic Communications of EASST *
% *********************************************************
\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{trkalray}[2006/09/29 TR Kalray document class v1.0]
% Derived from the article class, fixing it to be two-sided 11pt output
% on A4 paper
\LoadClass[a4paper,11pt,twoside]{article}
% =======
% Lengths
% =======
\setlength{\textwidth}{420pt}
\setlength{\headheight}{35pt}
\setlength{\oddsidemargin}{16pt}
\setlength{\evensidemargin}{16pt}
\setlength{\parindent}{1em}
\setlength{\parskip}{0pt}
% =========
% Footnotes
% =========
\renewcommand{\@makefntext}[1]{%
  \noindent\textsuperscript\@thefnmark\hspace{1em} #1}
% ====================================
% Spacing of figure and table captions
% ====================================
\setlength{\abovecaptionskip}{6pt}
\setlength{\belowcaptionskip}{6pt}
% ================================
% Environment for acknowledgements
% ================================
\newenvironment{acknowledge}{%
  \par\vspace{11pt}%
  \noindent\textbf{Acknowledgements:}\hspace{1em}}{%
  \par\vspace{11pt}}
% =============================
% Miscellaneous package imports
% =============================
\RequirePackage{mathptmx}
\RequirePackage[scaled=.90]{helvet}
\RequirePackage{courier}
% Too bad, this nice font isn't available everywhere:
%\RequirePackage{cmbright}
%\DeclareFontShape{OT1}{cmbr}{bx}{n}{%
%<->cmbrbx10%
%}{}
% Graphics
\RequirePackage{graphicx}
\RequirePackage{pgf}
% Colors
\RequirePackage{color}
\definecolor{kalraylightblue}{rgb}{.12,.568,.81}
\definecolor{kalrayblue}{rgb}{.058,.345,.55}
\definecolor{kalrayvioline}{rgb}{.43,.09,0.337}
\definecolor{grayshade}{rgb}{0.92,0.92,0.92}
% =========================================
% AMS packages for mathematics and theorems
% =========================================
\RequirePackage[tbtags,sumlimits,intlimits,namelimits,reqno,fleqn]{amsmath}
\RequirePackage{amsthm}
\RequirePackage{amsfonts}
\RequirePackage{amssymb}
% Custom theorem-like environments
\newtheoremstyle{ectheorem}{11pt}{11pt}{\itshape}{}{\bfseries}{}{1em}{}
\newtheoremstyle{ecdefinition}{11pt}{11pt}{\upshape}{}{\bfseries}{}{1em}{}
\newtheoremstyle{ecremark}{11pt}{11pt}{\slshape}{}{\itshape}{}{1em}{}
\theoremstyle{ectheorem}
\newtheorem{theorem}{Theorem}
\newtheorem{proposition}{Proposition}
\newtheorem{lemma}{Lemma}
\newtheorem{corollary}{Corollary}
\theoremstyle{ecdefinition}
\newtheorem{definition}{Definition}
\newtheorem{algorithm}{Algorithm}
\theoremstyle{ecremark}
\newtheorem{remark}{Remark}
\newtheorem{example}{Example}
% ======================
% Package for Hyperlinks
% ======================
% Hyperlinks are coloured in kalrayblue
\RequirePackage[colorlinks=true,linkcolor=kalrayblue,citecolor=kalrayblue,filecolor=kalrayblue,menucolor=kalrayblue,pagecolor=kalrayblue,urlcolor=kalrayblue,pdfpagemode={None},pdfsubject={Software Science and Technology},pdfstartview={FitH}]{hyperref}

\pgfdeclareimage[width=60pt]{kalray-logo}{kalray}
\pgfdeclareimage[width=70pt]{accesscore-logo}{accesscore}
% URLs should be in normal font
\urlstyle{same}
% Command for emails
\newcommand{\email}[1]{%
  \href{mailto:#1}{#1}}
% Command for Digital Object Identifiers
\newcommand{\doi}[1]{%
  \href{http://dx.doi.org/#1}{doi:#1}}
%%
% ==============================
% Package for listings of C code
% ==============================
\RequirePackage{listings}
\lstset{ %
language=C,                     % choose the language of the code
basicstyle=\footnotesize\ttfamily,  % the size&style of the fonts that are used for the code
backgroundcolor=\color{grayshade},  % background color. You must add \usepackage{color}
showspaces=false,               % show spaces adding particular underscores
showstringspaces=false,         % underline spaces within strings
showtabs=false,                 % show tabs within strings adding particular underscores
keywordstyle={\color{kalrayblue}},
stringstyle={\color{green!50!black}},
%frame=single,	                % adds a frame around the code
breaklines=true,                % sets automatic line breaking
breakatwhitespace=false,        % sets if automatic breaks should only happen at whitespace
%escapeinside={\%*}{*)}          % if you want to add a comment within your code
%morekeywords={*,...}            % if you want to add more keywords to the set
}
% Names to use in \autorefs
% -------------------------
% Names for sections and similar:
\renewcommand{\partautorefname}{Part}
\renewcommand{\sectionautorefname}{Section}
\renewcommand{\subsectionautorefname}{Subsection}
\renewcommand{\subsubsectionautorefname}{Subsubsection}
%% To avoid overlapping numbers on titles for hundreds of subsubsections
\renewcommand*\l@subsubsection{\@dottedtocline{3}{3.8em}{4.0em}}
% Names for theorem-like environments:
\renewcommand{\theoremautorefname}{Theorem}
\newcommand{\propositionautorefname}{Proposition}
\newcommand{\lemmaautorefname}{Lemma}
\newcommand{\corollaryautorefname}{Corollary}
\newcommand{\definitionautorefname}{Definition}
\newcommand{\algorithmautorefname}{Algorithm}
\newcommand{\exampleautorefname}{Example}
\newcommand{\remarkautorefname}{Remark}
% Names for figures and tables:
\renewcommand{\figureautorefname}{Figure}
\newcommand{\subfigureautorefname}{Figure}
\renewcommand{\tableautorefname}{Table}
\newcommand{\subtableautorefname}{Table}
% Name for the bibliography
\renewcommand{\refname}{Bibliography}
% Correct anchors for figures and tables to be above the float
% ------------------------------------------------------------
\RequirePackage[all]{hypcap}
% ====================
% Frontmatter commands
% ====================
% The version number and title
\newcommand{\trvernumber}{No \texttt{$\backslash$version} defined for report number!}
\newcommand{\trcopyyear}{No \texttt{$\backslash$version} defined for copyright year!}
\newcommand{\version}[3]{%
  \renewcommand{\trvernumber}{#1 #2}
  \renewcommand{\trcopyyear}{\copyright #3 Kalray}
  }
\newcommand{\trvertitle}{}
\newcommand{\versiontitle}[1]{%
  \renewcommand{\trvertitle}{#1}}
\newcommand{\trvershort}{\trvernumber}
\newcommand{\versionshort}[1]{%
  \renewcommand{\trvershort}{#1}}
% The version or guest editors
\newcommand{\treditor}{No \texttt{$\backslash$*ed(s)} defined!}
\newcommand{\voled}[1]{%
  \renewcommand{\treditor}{Volume Editor: #1}}
\newcommand{\voleds}[1]{%
  \renewcommand{\treditor}{Volume Editors: #1}}
\newcommand{\guested}[1]{%
  \renewcommand{\treditor}{Guest Editor: #1}}
\newcommand{\guesteds}[1]{%
  \renewcommand{\treditor}{Guest Editors: #1}}
% Title
\newcommand{\ectitle}{No \texttt{$\backslash$title} defined!}
\renewcommand{\title}[1]{%
  \renewcommand{\ectitle}{#1}%
  \hypersetup{pdftitle={#1}}}
% Version
\newcommand{\ecvers}{}
\newcommand{\vers}[1]{%
  \renewcommand{\ecvers}{Version #1}}
\newcommand{\ecshort}{\ectitle}
\newcommand{\short}[1]{%
  \renewcommand{\ecshort}{#1}}
% Author(s) and institutes
\newcommand{\ecauthor}{No \texttt{$\backslash$author} defined!}
\renewcommand{\author}[1]{%
  \renewcommand{\ecauthor}{#1}%
  \setcounter{page}{0}% Set to page 0 to avoid autrefs
  \hypersetup{pdfauthor={#1}}}
\newcommand{\autref}[1]{%
  \if \thepage 1% Only on page 1 (not on cover)
  \hyperlink{aut#1}{$^#1$}%
  \else\fi}
\newcommand{\ecdocowner}{No \texttt{$\backslash$docowner} defined!}
\newcommand{\docowner}[2]{%
  \ifx\usedocowner\undefined
    \renewcommand{\ecdocowner}{}
  \else
    \renewcommand{\ecdocowner}{Doc owner: #1 \textless\email{#2}\textgreater\\[11pt]}
  \fi
}
\newcommand{\ecinstitute}{No \texttt{$\backslash$institute} defined!}
\newcommand{\institute}[1]{%
  \renewcommand{\ecinstitute}{#1}}
\newcommand{\autlabel}[1]{%
  \hypertarget{aut#1}{$^#1$}}
% Abstract and keywords
\newcommand{\ecabstract}{No \texttt{$\backslash$abstract} defined!}
\renewcommand{\abstract}[1]{%
  \renewcommand{\ecabstract}{#1}}
\newcommand{\eckeywords}{No \texttt{$\backslash$keywords} defined!}
\newcommand{\keywords}[1]{%
  \renewcommand{\eckeywords}{#1}%
  \hypersetup{pdfkeywords={#1}}}
% ===================
% Headers and footers
% ===================
\RequirePackage{fancyhdr}
\pagestyle{fancy}
% ---------------------
% Odd/Even page headers
% ---------------------
\fancyhead{}
\fancyhead[LO]{\pgfuseimage{kalray-logo}}
\fancyhead[RE]{\pgfuseimage{accesscore-logo}}
\fancyhead[LE]{\textsf{\ecshort}}
\fancyhead[CO]{\textsf{\trvernumber}}
\fancyhead[RO]{\trcopyyear}

\renewcommand\headrule{{\color{kalrayblue}\hrule height 2pt}}
%{\vskip 3pt{\color{kalrayblue}\hrule height 2pt}}

% ---------------------
% Odd/Even page footer
% ---------------------
\fancyfoot{}
\fancyfoot[LE,RO]{\textsf{\thepage}}
\fancyfoot[RE,LO]{\textsf{Kalray Confidential --- all rights reserved}}

\renewcommand\footrule{{\color{kalrayblue}\hrule height 2pt}}

%\def\@evenfoot{\vbox{%
%  {\color{kalrayblue}\hrule height 2pt}
%  \vskip 3pt%
%  \textsf{\thepage}% on the left
%  \hfill%
%  \textsf{Kalray Confidential -- all rights reserved}}}% on the right

% Odd page footer
% ----------------
%\def\@oddfoot{\vbox{%
%  {\color{kalrayblue}\hrule height 2pt}
%  \vskip 3pt%
%  \textsf{Kalray Confidential -- all rights reserved}% on the left
%  \hfill%
%  \textsf{\thepage}}}% on the right
% ===========================================================
% Making the title page and the frontmatter on the first page
% ===========================================================
% Define a label for the last page
\AtEndDocument{%
  \clearpage%
  \addtocounter{page}{-1}%
  \immediate\write\@auxout{\string
  \newlabel{lastpage}{{}{\thepage}{}{}}}%
  \addtocounter{page}{1}}
% Introduce the maketitle command
\renewcommand{\maketitle}{%
  % The title page
  % --------------
  \thispagestyle{empty}
  \enlargethispage{60pt}
  % Will be page 0, s.t. contents start on page 1
  \setcounter{page}{0}
  % Title box
  \phantom{Phantom paragraph for vspace}\par\vspace{-85pt}
  \noindent \colorbox{kalrayblue}{\parbox{414pt}{\color{white}\sf
    \vspace{6pt}
    \hspace{6pt}{\LARGE \trvernumber \hfill \trcopyyear}
    \vspace{6pt}}}
  % EASST logo
  %BD3\begin{flushright}
  \begin{minipage}{0.5\linewidth}
  \begin{center}
  \includegraphics[height=32pt]{kalray}
  \end{center}
  \end{minipage}
  \begin{minipage}{0.5\linewidth}
  \begin{center}
    \vspace{0.4cm}
    \includegraphics[height=43pt]{accesscore}
  \end{center}
  \end{minipage}
  %\end{flushright}
  % Volume and article title, author(s)
  \vspace{60pt}
  \begin{center}
    {\LARGE\trvertitle\par}
    \vspace{44pt}
    {\Large\ectitle\par}
    \vspace{11pt}
    \ecauthor\\[11pt]
    \vspace{11pt}
    \ecdocowner\par
    \pageref*{lastpage} pages
  \end{center}
  % Information
  \begin{center}
  \vspace{30pt}
  \frame{\hphantom{m}\parbox{.7\textwidth}{\small\vphantom{I}
This document and the information therein are the exclusive property of Kalray
SA. They are disclosed to the Recipient within the strict scope of a
non-disclosure agreement between the Recipient and Kalray SA.
\\}\hphantom{m}}
  \end{center}
  \vspace{6pt}
  \begin{center}
  \ecvers
  \end{center}
  \vfill
  \colorbox{kalrayblue}{\parbox{414pt}{\color{white}\sf
    \vspace{6pt}
    \hspace*{6pt}{\large\textsf{Kalray confidential -- all rights reserved}}\\
    \hspace{6pt}
    \vspace{6pt}}}
  \newpage
  % Beginning of article title on first page
  % ----------------------------------------
  \global\@topnum\z@ % To prevent floats above the title
  % Information on title, author(s) and institutes of authors centered
  \setlength{\parskip}{11pt}
  \begin{center}
    {\Large\bfseries\ectitle\par}
    {\large\bfseries\ecauthor\par}
    \ecinstitute
  \end{center}
  % Abstract and keywords as a block quote
  \begin{quote}
    \textbf{Abstract:} \ecabstract\par
    \textbf{Keywords:} \eckeywords
  \end{quote}
  \setlength{\parskip}{0pt}}
